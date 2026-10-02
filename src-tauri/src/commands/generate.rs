//! Rendering commands: preview, copy and export.

use std::path::Path;

use base64::Engine;
use base64::engine::general_purpose::STANDARD as BASE64;

use crate::error::{err, err_with, AppResult, ErrorCode};
use crate::qr::render::{canvas_to_bgra, rasterize};
use crate::qr::{build_matrix, render, RenderRequest, ResolvedStyle};

/// Which file format an export uses.
#[derive(Debug, Clone, Copy, PartialEq, Eq, serde::Serialize, serde::Deserialize)]
#[serde(rename_all = "camelCase")]
pub enum ExportFormat {
    Png,
    Svg,
}

/// Render the preview shown in the main window, including scan verification.
#[tauri::command]
pub fn render_qr(request: RenderRequest) -> AppResult<crate::qr::RenderResult> {
    let mut request = request;
    request.size_px = request.validate()?;
    render(&request)
}

/// Copy the QR code to the Windows clipboard as a real bitmap.
///
/// The clipboard receives `CF_DIB`, which Paint, Word, Slack and browsers all
/// accept as a normal image.
#[tauri::command]
pub fn copy_qr(request: RenderRequest) -> AppResult<()> {
    let mut request = request;
    request.size_px = request.validate()?;

    let result = render(&request)?;
    let decoded = BASE64
        .decode(&result.png_base64)
        .map_err(|error| err_with(ErrorCode::ClipboardFailed, error))?;
    let canvas = decode_png(&decoded)?;

    let mut clipboard = arboard::Clipboard::new()
        .map_err(|error| err_with(ErrorCode::ClipboardFailed, error))?;
    // The Windows clipboard expects `CF_DIB`, which arboard builds from BGRA
    // bytes; Paint, Word and browsers all read that as a normal image.
    let image = arboard::ImageData {
        width: canvas.width as usize,
        height: canvas.height as usize,
        bytes: std::borrow::Cow::Owned(canvas_to_bgra(&canvas)),
    };
    clipboard
        .set_image(image)
        .map_err(|error| err_with(ErrorCode::ClipboardFailed, error))?;
    Ok(())
}

/// Read plain text from the clipboard. Called once at start-up only.
#[tauri::command]
pub fn read_clipboard_text() -> AppResult<Option<String>> {
    match arboard::Clipboard::new() {
        Ok(mut clipboard) => match clipboard.get_text() {
            Ok(text) => Ok(Some(text)),
            // An empty clipboard, or one holding only an image, is not an error.
            Err(_) => Ok(None),
        },
        Err(_) => Ok(None),
    }
}

/// Write a rendered QR code to disk in the requested format.
#[tauri::command]
pub fn export_qr(
    request: RenderRequest,
    path: String,
    format: ExportFormat,
) -> AppResult<()> {
    let target = Path::new(&path);
    if path.trim().is_empty() {
        return Err(err(ErrorCode::FileWriteFailed));
    }

    let mut request = request;
    request.size_px = request.validate()?;
    let payload = crate::qr::encode_payload(&request.payload)?;
    let mut resolved = request.style.resolve()?;
    let mut ec_level = request.ec_level;
    if resolved.logo.is_some() && ec_level != crate::qr::payload::EcLevel::H {
        ec_level = crate::qr::payload::EcLevel::H;
    }

    if let Some(input) = &request.style.logo {
        resolved.logo = Some(crate::qr::logo::LogoAsset::decode(input)?);
    }

    let matrix = build_matrix(&payload, ec_level)?;
    let bytes = match format {
        ExportFormat::Png => {
            let canvas = rasterize(&matrix, &resolved, request.size_px);
            png_bytes(&canvas)?
        }
        ExportFormat::Svg => {
            crate::qr::svg::export(&matrix, &resolved, request.size_px)?.into_bytes()
        }
    };

    std::fs::write(target, bytes)
        .map_err(|error| err_with(ErrorCode::FileWriteFailed, error))?;
    Ok(())
}

/// Preview size used for the main window. Always rendered at module aligned
/// integer pixels so the preview and the export are pixel identical in shape.
pub const PREVIEW_SIZE: u32 = 1024;

/// Style defaults shared with the front end, exposed so both stay in sync.
#[tauri::command]
pub fn preview_defaults() -> AppResult<crate::qr::QrStyle> {
    let style = crate::qr::QrStyle::default();
    // Validate the defaults so a bad default can never reach the renderer.
    let _: ResolvedStyle = style.resolve()?;
    Ok(style)
}

fn png_bytes(canvas: &crate::qr::render::Canvas) -> AppResult<Vec<u8>> {
    let base64 = crate::qr::render::encode_png(canvas)?;
    BASE64
        .decode(base64)
        .map_err(|error| err_with(ErrorCode::RenderFailed, error))
}

fn decode_png(bytes: &[u8]) -> AppResult<crate::qr::render::Canvas> {
    let image = image::load_from_memory(bytes)
        .map_err(|error| err_with(ErrorCode::RenderFailed, error))?
        .to_rgba8();
    let (width, height) = image.dimensions();
    Ok(crate::qr::render::Canvas { width, height, pixels: image.into_raw() })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::qr::payload::{EcLevel, QrPayload};
    use crate::qr::QrStyle;

    fn request() -> RenderRequest {
        RenderRequest {
            payload: QrPayload::Text { text: "VYNX QR".into() },
            style: QrStyle::default(),
            ec_level: EcLevel::H,
            size_px: 512,
        }
    }

    #[test]
    fn export_writes_a_png() {
        let dir = tempfile::tempdir().expect("tempdir");
        let target = dir.path().join("qr.png");
        export_qr(request(), target.to_string_lossy().into_owned(), ExportFormat::Png)
            .expect("export png");
        let bytes = std::fs::read(&target).expect("read");
        assert_eq!(&bytes[1..4], b"PNG");
    }

    #[test]
    fn export_writes_an_svg() {
        let dir = tempfile::tempdir().expect("tempdir");
        let target = dir.path().join("qr.svg");
        export_qr(request(), target.to_string_lossy().into_owned(), ExportFormat::Svg)
            .expect("export svg");
        let text = std::fs::read_to_string(&target).expect("read");
        assert!(text.contains("<svg"));
        assert!(text.trim_end().ends_with("</svg>"));
    }

    #[test]
    fn export_reports_unwritable_targets() {
        let error = export_qr(
            request(),
            "Z:\\definitely\\not\\writable\\qr.png".to_string(),
            ExportFormat::Png,
        )
        .unwrap_err();
        assert_eq!(error.code(), ErrorCode::FileWriteFailed);
    }

    #[test]
    fn export_rejects_empty_paths() {
        let error = export_qr(request(), String::new(), ExportFormat::Png).unwrap_err();
        assert_eq!(error.code(), ErrorCode::FileWriteFailed);
    }
}
