//! Rendering entry points.
//!
//! These are the operations the UI performs. They return bytes or text and never
//! touch the filesystem or the clipboard: writing files and putting data on the
//! clipboard are the caller's business, which keeps this layer portable and the
//! error handling honest.

use crate::error::{err_with, AppResult, ErrorCode};
use crate::qr::logo::LogoAsset;
use crate::qr::payload::QrPayload;
use crate::qr::render::Canvas;
use crate::qr::svg;
use crate::qr::{build_matrix, render, RenderRequest, RenderResult, ResolvedStyle};
use crate::{MAX_LOGO_BYTES, MAX_LOGO_DIMENSION, MAX_RENDER_SIZE};

/// Render a preview: encode, build, rasterise, verify.
///
/// Returns raw PNG bytes rather than a data URL. There is no base64 round trip
/// between the engine and the window, and there never was a reason for one.
pub fn render_png(request: &RenderRequest) -> AppResult<RenderResult> {
    let mut request = request.clone();
    request.size_px = validate(request.size_px)?;
    render(&request)
}

/// Render the vector form at the requested size.
pub fn render_svg(request: &RenderRequest, size_px: u32) -> AppResult<String> {
    let size = validate(size_px)?;
    let payload = crate::qr::encode_payload(&request.payload)?;
    let mut resolved = request.style.resolve()?;
    if let Some(input) = &request.style.logo {
        resolved.logo = Some(LogoAsset::decode(input)?);
    }

    let matrix = build_matrix(&payload, effective_ec(request))?;
    svg::export(&matrix, &resolved, size)
}

/// Render straight to RGBA, for putting an image on the clipboard.
///
/// Re-rendering rather than decoding the PNG keeps the pixels exactly as the
/// renderer produced them.
pub fn render_rgba(request: &RenderRequest) -> AppResult<Canvas> {
    let mut request = request.clone();
    request.size_px = validate(request.size_px)?;
    let resolved = resolve(&request)?;
    let payload = crate::qr::encode_payload(&request.payload)?;
    let matrix = build_matrix(&payload, effective_ec(&request))?;
    Ok(crate::qr::render::rasterize(&matrix, &resolved, request.size_px))
}

/// Read a logo file from disk, refusing anything oversized before decoding.
///
/// The size check comes first on purpose: decoding is the expensive step, and a
/// caller should not be able to make the process allocate gigabytes by pointing
/// it at a small file that expands enormously.
pub fn load_logo(path: &str) -> AppResult<crate::qr::logo::LogoAsset> {
    let trimmed = path.trim();
    if trimmed.is_empty() {
        return Err(err_with(ErrorCode::LogoDecodeFailed, "empty path"));
    }

    let file = std::path::Path::new(trimmed);
    if !file.is_file() {
        return Err(err_with(ErrorCode::FileReadFailed, trimmed));
    }

    let metadata =
        std::fs::metadata(file).map_err(|error| err_with(ErrorCode::FileReadFailed, error))?;
    if metadata.len() > MAX_LOGO_BYTES as u64 {
        return Err(err_with(ErrorCode::LogoTooBig, metadata.len()));
    }

    let bytes =
        std::fs::read(file).map_err(|error| err_with(ErrorCode::FileReadFailed, error))?;
    let asset = LogoAsset::from_bytes(file_name(file), &bytes)?;
    check_logo_dimensions(asset.width, asset.height)?;
    Ok(asset)
}

/// Reject a decoded logo that is larger than the renderer can use.
///
/// Decoding succeeds long before the pixel count becomes absurd, so this runs
/// afterwards on the dimensions the decoder actually produced.
pub fn check_logo_dimensions(width: u32, height: u32) -> AppResult<()> {
    if width == 0 || height == 0 {
        return Err(err_with(ErrorCode::LogoDecodeFailed, "image has no pixels"));
    }
    if width > MAX_LOGO_DIMENSION || height > MAX_LOGO_DIMENSION {
        return Err(err_with(
            ErrorCode::LogoTooBig,
            format!("{width}x{height} exceeds {MAX_LOGO_DIMENSION}px"),
        ));
    }
    Ok(())
}

/// True when the payload can be encoded, for inline form validation.
pub fn is_encodable(payload: &QrPayload) -> bool {
    payload.encode().is_ok()
}

/// Error correction actually used, which is High whenever a logo is present.
fn effective_ec(request: &RenderRequest) -> crate::qr::payload::EcLevel {
    use crate::qr::payload::EcLevel;
    if request.style.has_logo() {
        EcLevel::H
    } else {
        request.ec_level
    }
}

/// Resolve the style and attach a decoded logo, if there is one.
fn resolve(request: &RenderRequest) -> AppResult<ResolvedStyle> {
    let mut resolved = request.style.resolve()?;
    if let Some(input) = &request.style.logo {
        let asset = LogoAsset::decode(input)?;
        check_logo_dimensions(asset.width, asset.height)?;
        resolved.logo = Some(asset);
    }
    Ok(resolved)
}

/// Clamp and validate a requested side length.
fn validate(size_px: u32) -> AppResult<u32> {
    let size = if size_px == 0 { 1024 } else { size_px };
    if size > MAX_RENDER_SIZE {
        return Err(err_with(
            ErrorCode::RenderFailed,
            format!("requested size exceeds {MAX_RENDER_SIZE} px"),
        ));
    }
    Ok(size)
}

fn file_name(file: &std::path::Path) -> String {
    file.file_name()
        .map(|value| value.to_string_lossy().into_owned())
        .unwrap_or_else(|| "logo".to_string())
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::qr::payload::EcLevel;
    use crate::qr::QrStyle;

    fn request() -> RenderRequest {
        crate::request_for("VYNX QR")
    }

    #[test]
    fn renders_raw_png_bytes() {
        let result = render_png(&request()).expect("render");
        // PNG magic number.
        assert_eq!(&result.png[..4], b"\x89PNG");
    }

    #[test]
    fn renders_svg_without_a_logo_data_uri() {
        let document = render_svg(&request(), 512).expect("svg");
        assert!(document.contains("<svg"));
        assert!(document.trim_end().ends_with("</svg>"));
        assert!(!document.contains("data:image/png;base64"));
    }

    #[test]
    fn rejects_sizes_beyond_the_limit() {
        let mut request = request();
        request.size_px = MAX_RENDER_SIZE + 1;
        let error = render_png(&request).unwrap_err();
        assert_eq!(error.code(), ErrorCode::RenderFailed);
    }

    #[test]
    fn clamps_a_zero_size_to_a_default() {
        let mut request = request();
        request.size_px = 0;
        assert_eq!(render_png(&request).expect("render").width, 1024);
    }

    #[test]
    fn clipboard_rendering_produces_rgba() {
        let canvas = render_rgba(&request()).expect("rgba");
        assert_eq!(canvas.width, canvas.height);
        assert_eq!(canvas.pixels.len(), (canvas.width * canvas.height * 4) as usize);
    }

    #[test]
    fn logo_dimensions_are_capped() {
        assert!(check_logo_dimensions(512, 512).is_ok());
        assert!(check_logo_dimensions(MAX_LOGO_DIMENSION, 16).is_ok());
        assert!(check_logo_dimensions(MAX_LOGO_DIMENSION + 1, 16).is_err());
        assert!(check_logo_dimensions(0, 16).is_err());
    }

    #[test]
    fn loading_a_missing_logo_reports_a_read_failure() {
        let error = load_logo("Z:\\missing\\logo.png").unwrap_err();
        assert!(matches!(
            error.code(),
            ErrorCode::FileReadFailed | ErrorCode::LogoDecodeFailed
        ));
    }

    #[test]
    fn loading_a_blank_path_is_rejected() {
        assert_eq!(load_logo("   ").unwrap_err().code(), ErrorCode::LogoDecodeFailed);
    }

    #[test]
    fn a_logo_raises_error_correction_to_high() {
        let mut request = request();
        request.ec_level = EcLevel::L;
        request.style = QrStyle { logo: Some(logo()), ..QrStyle::default() };
        let result = render_png(&request).expect("render");
        assert_eq!(result.ec_level, EcLevel::H);
        assert!(result.ec_adjusted);
    }

    /// The bump used to be decided from `resolved.logo` before the logo had been
    /// decoded, so `ec_adjusted` was never true and the level was never raised.
    #[test]
    fn a_logo_reports_the_bump_in_warnings() {
        let mut request = request();
        request.ec_level = EcLevel::M;
        request.style = QrStyle { logo: Some(logo()), ..QrStyle::default() };
        let result = render_png(&request).expect("render");
        assert!(result.warnings.iter().any(|w| w.code == "ecAdjusted"));
    }

    #[test]
    fn no_logo_leaves_the_chosen_level_alone() {
        let mut request = request();
        request.ec_level = EcLevel::L;
        let result = render_png(&request).expect("render");
        assert_eq!(result.ec_level, EcLevel::L);
        assert!(!result.ec_adjusted);
    }

    /// A one pixel PNG, which is the smallest logo the decoder accepts.
    fn logo() -> crate::qr::LogoInput {
        use crate::qr::render::encode_png;
        use crate::qr::Rgba;
        let canvas = Canvas::new(1, 1, Rgba::new(0, 0, 0, 255));
        crate::qr::LogoInput {
            name: "dot.png".into(),
            data: encode_png(&canvas).expect("png"),
        }
    }

    #[test]
    fn encodability_is_reported_without_throwing() {
        assert!(is_encodable(&QrPayload::Text { text: "ok".into() }));
        assert!(!is_encodable(&QrPayload::Text { text: "   ".into() }));
    }
}