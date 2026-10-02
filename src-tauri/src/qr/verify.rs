//! Scan verification.
//!
//! Every QR code VYNX QR shows is decoded again by an independent decoder before
//! it reaches the user interface. The status line reports what actually happened,
//! so `Scan verified` is a measured fact rather than a decoration.

use serde::{Deserialize, Serialize};

use crate::qr::render::Canvas;

/// Result of the internal round trip.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub enum VerifyStatus {
    /// Decoded and the payload matched byte for byte.
    Verified,
    /// The decoder could not read the symbol.
    Failed,
    /// The decoder read something else entirely.
    Mismatch,
}

impl VerifyStatus {
    pub const fn is_verified(self) -> bool {
        matches!(self, Self::Verified)
    }
}

/// Verification outcome reported to the UI.
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct Verification {
    pub status: VerifyStatus,
    /// Payload the decoder recovered. Only populated on failure, truncated.
    pub decoded: Option<String>,
    /// Result of the reduced size check, when a logo is applied.
    pub reduced: Option<VerifyStatus>,
}

impl Verification {
    fn failed() -> Self {
        Self { status: VerifyStatus::Failed, decoded: None, reduced: None }
    }
}

/// Decode a rendered canvas back into text.
pub fn decode_canvas(canvas: &Canvas) -> Option<String> {
    if canvas.width < 21 || canvas.height < 21 {
        return None;
    }
    let width = canvas.width as usize;
    let height = canvas.height as usize;
    let grey = canvas.to_greyscale();
    let mut prepared = rqrr::PreparedImage::prepare_from_greyscale(width, height, |x, y| {
        grey.get(y * width + x).copied().unwrap_or(255)
    });
    let grid = prepared.detect_grids().into_iter().next()?;
    let (_, content) = grid.decode().ok()?;
    Some(content)
}

/// Decode `canvas` and compare it with `expected`.
pub fn verify_payload(expected: &str, canvas: &Canvas) -> Verification {
    match decode_canvas(canvas) {
        Some(decoded) if decoded == expected => Verification {
            status: VerifyStatus::Verified,
            decoded: None,
            reduced: None,
        },
        Some(decoded) => Verification {
            status: VerifyStatus::Mismatch,
            decoded: Some(truncate(&decoded)),
            reduced: None,
        },
        None => Verification::failed(),
    }
}

fn truncate(value: &str) -> String {
    const LIMIT: usize = 160;
    if value.chars().count() <= LIMIT {
        return value.to_string();
    }
    value.chars().take(LIMIT).collect::<String>() + "…"
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::qr::payload::{EcLevel, ModuleStyle};
    use crate::qr::render::rasterize;
    use crate::qr::{build_matrix, QrStyle};

    fn render(payload: &str, style: &QrStyle, ec: EcLevel) -> Canvas {
        let matrix = build_matrix(payload, ec).expect("matrix");
        let resolved = style.resolve().expect("style");
        rasterize(&matrix, &resolved, 600)
    }

    #[test]
    fn round_trips_a_url() {
        let canvas = render("https://github.com/VYNX", &QrStyle::default(), EcLevel::M);
        assert_eq!(decode_canvas(&canvas).as_deref(), Some("https://github.com/VYNX"));
        assert!(verify_payload("https://github.com/VYNX", &canvas).status.is_verified());
    }

    #[test]
    fn round_trips_rounded_modules() {
        let style = QrStyle { module_style: ModuleStyle::Rounded, ..QrStyle::default() };
        let canvas = render("https://github.com/VYNX", &style, EcLevel::M);
        assert!(verify_payload("https://github.com/VYNX", &canvas).status.is_verified());
    }

    #[test]
    fn round_trips_unicode() {
        let text = "Привіт, Україно 🇺🇦";
        let canvas = render(text, &QrStyle::default(), EcLevel::H);
        assert_eq!(decode_canvas(&canvas).as_deref(), Some(text));
    }

    #[test]
    fn detects_mismatch() {
        let canvas = render("https://example.com", &QrStyle::default(), EcLevel::M);
        let verification = verify_payload("https://example.org", &canvas);
        assert_eq!(verification.status, VerifyStatus::Mismatch);
        assert_eq!(verification.decoded.as_deref(), Some("https://example.com"));
    }

    #[test]
    fn detects_inverted_colours_as_unreadable() {
        let style = QrStyle { foreground: "#FFFFFF".into(), background: "#000000".into(), ..QrStyle::default() };
        let canvas = render("https://example.com", &style, EcLevel::M);
        let verification = verify_payload("https://example.com", &canvas);
        assert_eq!(verification.status, VerifyStatus::Failed);
    }
}
