//! QR code decoding from images or raw pixel buffers.

use crate::error::{err, err_with, AppResult, ErrorCode};

/// Decode a QR code from a raw RGBA8 pixel buffer (4 bytes per pixel: R, G, B, A).
pub fn decode_rgba(width: u32, height: u32, rgba: &[u8]) -> AppResult<String> {
    if width < 21 || height < 21 {
        return Err(err_with(
            ErrorCode::RenderFailed,
            "Image dimensions are too small for a QR code",
        ));
    }
    let expected_len = (width as usize) * (height as usize) * 4;
    if rgba.len() < expected_len {
        return Err(err_with(
            ErrorCode::RenderFailed,
            "Pixel buffer is smaller than width * height * 4",
        ));
    }

    let w = width as usize;
    let h = height as usize;

    let mut prepared = rqrr::PreparedImage::prepare_from_greyscale(w, h, |x, y| {
        let idx = (y * w + x) * 4;
        let r = rgba[idx] as u32;
        let g = rgba[idx + 1] as u32;
        let b = rgba[idx + 2] as u32;
        let a = rgba[idx + 3] as u32;

        if a < 128 {
            255 // Treat transparent areas as white background
        } else {
            // Standard perceptual luminance formula: 0.299 R + 0.587 G + 0.114 B
            ((r * 299 + g * 587 + b * 114) / 1000) as u8
        }
    });

    let grids = prepared.detect_grids();
    if grids.is_empty() {
        return Err(err(ErrorCode::VerificationFailed));
    }

    for grid in grids {
        if let Ok((_, content)) = grid.decode() {
            return Ok(content);
        }
    }

    Err(err(ErrorCode::VerificationFailed))
}

/// Decode a QR code from raw encoded image bytes (PNG, JPEG, WebP).
pub fn decode_bytes(bytes: &[u8]) -> AppResult<String> {
    let img = image::load_from_memory(bytes).map_err(|e| {
        err_with(
            ErrorCode::LogoDecodeFailed,
            format!("Failed to read image: {e}"),
        )
    })?;

    let luma = img.to_luma8();
    let (width, height) = luma.dimensions();
    let w = width as usize;
    let h = height as usize;
    let raw = luma.into_raw();

    let mut prepared = rqrr::PreparedImage::prepare_from_greyscale(w, h, |x, y| {
        raw.get(y * w + x).copied().unwrap_or(255)
    });

    let grids = prepared.detect_grids();
    if grids.is_empty() {
        return Err(err(ErrorCode::VerificationFailed));
    }

    for grid in grids {
        if let Ok((_, content)) = grid.decode() {
            return Ok(content);
        }
    }

    Err(err(ErrorCode::VerificationFailed))
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::qr::payload::EcLevel;
    use crate::qr::render::rasterize;
    use crate::qr::{build_matrix, QrStyle};

    #[test]
    fn round_trip_decode_rgba() {
        let text = "https://github.com/Maks0101aps/VYNX-QR";
        let matrix = build_matrix(text, EcLevel::M).expect("matrix");
        let style = QrStyle::default().resolve().expect("style");
        let canvas = rasterize(&matrix, &style, 300);

        let decoded = decode_rgba(canvas.width, canvas.height, &canvas.pixels).expect("decode");
        assert_eq!(decoded, text);
    }
}
