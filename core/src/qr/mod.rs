//! QR engine: matrix construction, raster rendering, vector export and the
//! built-in scan verification.

pub mod logo;
pub mod payload;
pub mod render;
pub mod svg;
pub mod verify;

pub use logo::{LogoAsset, LogoInput};
pub use render::{Canvas, Rgba};
pub use verify::{Verification, VerifyStatus};

use crate::error::{err, err_with, AppResult, ErrorCode};
use crate::qr::payload::{EcLevel, ModuleStyle, QrPayload};

/// Minimum quiet zone required by the QR specification.
pub const MIN_QUIET_ZONE: u32 = 1;
/// Default quiet zone in modules.
pub const DEFAULT_QUIET_ZONE: u32 = 4;
/// Largest quiet zone the UI allows.
pub const MAX_QUIET_ZONE: u32 = 8;

/// Export sizes offered in the UI.
pub const EXPORT_SIZES: [u32; 4] = [256, 512, 1024, 2048];

/// A square grid of QR modules. `true` means "dark".
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct QrMatrix {
    /// Modules per side, excluding the quiet zone.
    pub size: usize,
    /// QR version (1-40, or 1-4 for micro).
    pub version: u8,
    /// Row-major module grid, `size * size` entries.
    pub modules: Vec<bool>,
}

impl QrMatrix {
    #[inline]
    pub fn get(&self, x: usize, y: usize) -> bool {
        self.modules[y * self.size + x]
    }

    /// True when the module belongs to one of the three position detection
    /// patterns. Rounded styling treats these separately so they stay scannable.
    pub fn is_finder(&self, x: usize, y: usize) -> bool {
        let n = self.size;
        let far = n.saturating_sub(7);
        (x < 7 && y < 7) || (x >= far && y < 7) || (x < 7 && y >= far)
    }
}

/// Build a QR matrix for `payload` at the requested error correction level.
pub fn build_matrix(payload: &str, ec: EcLevel) -> AppResult<QrMatrix> {
    let code = qrcode::QrCode::with_error_correction_level(payload.as_bytes(), ec.into())
        .map_err(|error| match error {
            qrcode::types::QrError::DataTooLong => err_with(
                ErrorCode::ContentTooLong,
                format!("{} bytes exceeds capacity at {}", payload.len(), ec.label()),
            ),
            other => err_with(ErrorCode::RenderFailed, other),
        })?;

    // `QrCode::width()` already excludes the quiet zone.
    let size = code.width();
    if size < 21 || size % 4 != 1 {
        return Err(err_with(ErrorCode::RenderFailed, "unexpected matrix geometry"));
    }
    let grid = code.to_colors();
    if grid.len() != size * size {
        return Err(err_with(ErrorCode::RenderFailed, "unexpected matrix geometry"));
    }

    let modules = grid
        .into_iter()
        .map(|color| color == qrcode::Color::Dark)
        .collect();

    // Only standard versions are produced, where modules = 17 + 4 * version.
    let version = ((size - 17) / 4) as u8;

    Ok(QrMatrix { size, version, modules })
}

/// Everything that influences the appearance of a QR code.
#[derive(Debug, Clone, PartialEq)]
pub struct QrStyle {
    pub module_style: ModuleStyle,
    /// `#RRGGBB`
    pub foreground: String,
    /// `#RRGGBB`
    pub background: String,
    /// Quiet zone width in modules.
    pub quiet_zone: u32,
    /// Fraction of the QR width covered by the logo, 0.05 - 0.30.
    pub logo_ratio: f32,
    pub logo: Option<LogoInput>,
}

impl Default for QrStyle {
    fn default() -> Self {
        Self {
            module_style: ModuleStyle::default(),
            foreground: "#000000".to_string(),
            background: "#FFFFFF".to_string(),
            quiet_zone: DEFAULT_QUIET_ZONE,
            logo_ratio: 0.20,
            logo: None,
        }
    }
}

impl QrStyle {
    pub fn has_logo(&self) -> bool {
        self.logo.is_some()
    }

    pub fn foreground_rgba(&self) -> AppResult<Rgba> {
        Rgba::from_hex(&self.foreground)
    }

    pub fn background_rgba(&self) -> AppResult<Rgba> {
        Rgba::from_hex(&self.background)
    }

    /// Resolve and validate the style, returning parsed colours and a clamped
    /// quiet zone.
    pub fn resolve(&self) -> AppResult<ResolvedStyle> {
        let foreground = self.foreground_rgba()?;
        let background = self.background_rgba()?;
        let quiet_zone = self.quiet_zone.clamp(MIN_QUIET_ZONE, MAX_QUIET_ZONE);
        let logo_ratio = if self.logo.is_some() {
            self.logo_ratio.clamp(0.05, 0.30)
        } else {
            0.0
        };
        Ok(ResolvedStyle {
            module_style: self.module_style,
            foreground,
            background,
            quiet_zone,
            logo_ratio,
            logo: None,
        })
    }
}

/// A validated style ready for rendering.
#[derive(Debug, Clone, PartialEq)]
pub struct ResolvedStyle {
    pub module_style: ModuleStyle,
    pub foreground: Rgba,
    pub background: Rgba,
    pub quiet_zone: u32,
    pub logo_ratio: f32,
    pub logo: Option<LogoAsset>,
}

/// A non-fatal problem the user should know about.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Warning {
    pub code: String,
    pub message: String,
}

impl Warning {
    pub fn new(code: &str, message: impl Into<String>) -> Self {
        Self { code: code.to_string(), message: message.into() }
    }
}

/// Everything the UI needs to render a preview in one round trip.
#[derive(Debug, Clone, PartialEq)]
pub struct RenderResult {
    /// Raw PNG bytes, ready to hand to an image decoder.
    pub png: Vec<u8>,
    pub width: u32,
    pub height: u32,
    /// Modules per side, excluding the quiet zone.
    pub modules: u32,
    /// Total modules including the quiet zone.
    pub total_modules: u32,
    pub quiet_zone: u32,
    pub version: u8,
    /// Effective error correction level, after the automatic logo bump.
    pub ec_level: EcLevel,
    /// True when adding a logo raised the error correction level to High.
    pub ec_adjusted: bool,
    /// WCAG contrast ratio between foreground and background.
    pub contrast: f32,
    pub verification: Verification,
    pub warnings: Vec<Warning>,
    /// The exact string stored in the symbol.
    pub encoded: String,
}

/// Render a payload end to end: encode, build, rasterise, verify.
pub fn render(request: &RenderRequest) -> AppResult<RenderResult> {
    let resolved = request.style.resolve()?;
    let mut warnings: Vec<Warning> = Vec::new();

    // The logo has to be decoded before the error correction is decided, because
    // that decision depends on whether there is one. Checking `resolved.logo` first
    // always saw `None`, so the bump never happened.
    let mut resolved = resolved;
    resolved.logo = match &request.style.logo {
        Some(input) => Some(LogoAsset::decode(input)?),
        None => None,
    };

    let mut ec_level = request.ec_level;
    let mut ec_adjusted = false;
    if resolved.logo.is_some() && ec_level != EcLevel::H {
        ec_level = EcLevel::H;
        ec_adjusted = true;
        warnings.push(Warning::new(
            "ecAdjusted",
            "Error correction raised to High to protect the logo area.",
        ));
    }

    let payload = encode_payload(&request.payload)?;
    let matrix = build_matrix(&payload, ec_level)?;

    if resolved.logo_ratio > 0.25 {
        warnings.push(Warning::new(
            "logoTooLarge",
            crate::error::messages::LOGO_TOO_LARGE.to_string(),
        ));
    }

    let contrast = contrast_ratio(resolved.foreground, resolved.background);
    if contrast < 2.0 {
        warnings.push(Warning::new(
            "contrastCritical",
            "Very low contrast — this QR may be impossible to scan.",
        ));
    } else if contrast < 3.0 {
        warnings.push(Warning::new(
            "contrastLow",
            "Low contrast — this QR may be difficult to scan.",
        ));
    }

    let canvas = render::rasterize(&matrix, &resolved, request.size_px);

    let mut verification = verify::verify_payload(&payload, &canvas);
    if resolved.logo.is_some() {
        // A logo that only survives at full resolution is not reliable in the
        // wild, where a QR is often shown much smaller than the preview.
        let small = render::rasterize(&matrix, &resolved, (request.size_px / 2).max(160));
        let reduced = verify::verify_payload(&payload, &small);
        if reduced.status != VerifyStatus::Verified {
            verification.reduced = Some(reduced.status);
            warnings.push(Warning::new(
                "logoTooLarge",
                crate::error::messages::LOGO_TOO_LARGE.to_string(),
            ));
        }
        if verification.status != VerifyStatus::Verified && reduced.status == VerifyStatus::Verified {
            verification.status = VerifyStatus::Failed;
        }
    }

    if verification.status == VerifyStatus::Mismatch {
        warnings.push(Warning::new("verificationMismatch", crate::error::messages::VERIFICATION_FAILED));
    }

    let png = render::encode_png(&canvas)
        .map_err(|error| err_with(ErrorCode::RenderFailed, error))?;

    let total_modules = matrix.size as u32 + 2 * resolved.quiet_zone;

    Ok(RenderResult {
        png,
        width: canvas.width,
        height: canvas.height,
        modules: matrix.size as u32,
        total_modules,
        quiet_zone: resolved.quiet_zone,
        version: matrix.version,
        ec_level,
        ec_adjusted,
        contrast,
        verification,
        warnings,
        encoded: payload,
    })
}

/// Everything the renderer needs for one image.
#[derive(Debug, Clone, PartialEq)]
pub struct RenderRequest {
    pub payload: QrPayload,
    pub style: QrStyle,
    pub ec_level: EcLevel,
    /// Side length of the returned PNG in pixels.
    pub size_px: u32,
}

impl RenderRequest {
    /// Check the payload is encodable and return a usable side length.
    pub fn validate(&self) -> AppResult<u32> {
        crate::qr::payload::ensure_not_empty(&self.payload)?;
        let size = if self.size_px == 0 { 1024 } else { self.size_px };
        if size > crate::MAX_RENDER_SIZE {
            return Err(err_with(
                ErrorCode::RenderFailed,
                format!("requested size exceeds {} px", crate::MAX_RENDER_SIZE),
            ));
        }
        Ok(size)
    }
}

/// Encode a payload, mapping "too long" onto a friendly message.
pub fn encode_payload(payload: &QrPayload) -> AppResult<String> {
    payload.encode()
}

/// WCAG relative luminance contrast ratio between two colours.
pub fn contrast_ratio(a: Rgba, b: Rgba) -> f32 {
    let la = a.relative_luminance();
    let lb = b.relative_luminance();
    let (lighter, darker) = if la >= lb { (la, lb) } else { (lb, la) };
    ((lighter + 0.05) / (darker + 0.05)).min(21.0)
}

/// Small helper so commands can fail cleanly when a payload is unusable.
pub fn ensure_encodable(payload: &QrPayload) -> AppResult<String> {
    if payload.encode().is_err() {
        return Err(err(ErrorCode::EmptyInput));
    }
    Ok(payload.kind().to_string())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn builds_a_matrix_for_a_short_url() {
        let matrix = build_matrix("https://github.com", EcLevel::M).expect("matrix");
        // 18 bytes plus the mode and length headers fit version 2 at level M.
        assert_eq!(matrix.size, 25);
        assert_eq!(matrix.version, 2);
        assert_eq!(matrix.modules.len(), 25 * 25);
        assert!(matrix.get(0, 0), "top left finder must be dark");
        assert!(!matrix.get(7, 0), "separator must be light");
    }

    #[test]
    fn builds_the_smallest_version_for_tiny_payloads() {
        let matrix = build_matrix("VYNX", EcLevel::M).expect("matrix");
        assert_eq!(matrix.size, 21);
        assert_eq!(matrix.version, 1);
    }

    #[test]
    fn reports_oversized_payloads() {
        let payload = "x".repeat(8000);
        let error = build_matrix(&payload, EcLevel::H).unwrap_err();
        assert_eq!(error.code(), ErrorCode::ContentTooLong);
    }

    #[test]
    fn finder_detection_marks_the_three_corners() {
        let matrix = build_matrix("https://github.com", EcLevel::M).expect("matrix");
        let n = matrix.size;
        assert!(matrix.is_finder(0, 0));
        assert!(matrix.is_finder(n - 1, 0));
        assert!(matrix.is_finder(0, n - 1));
        assert!(!matrix.is_finder(n / 2, n / 2));
    }

    #[test]
    fn contrast_ratio_matches_known_values() {
        let black = Rgba::from_hex("#000000").expect("black");
        let white = Rgba::from_hex("#FFFFFF").expect("white");
        assert!((contrast_ratio(black, white) - 21.0).abs() < 0.001);
        let grey = Rgba::from_hex("#777777").expect("grey");
        assert!(contrast_ratio(grey, white) < 5.0);
    }

    #[test]
    fn renders_a_verified_qr() {
        let request = RenderRequest {
            payload: QrPayload::Text { text: "VYNX QR".into() },
            style: QrStyle::default(),
            ec_level: EcLevel::M,
            size_px: 512,
        };
        let result = render(&request).expect("render");
        assert_eq!(result.encoded, "VYNX QR");
        assert_eq!(result.width, result.height);
        assert!(result.verification.status == VerifyStatus::Verified);
        assert!(result.warnings.is_empty());
    }
}
