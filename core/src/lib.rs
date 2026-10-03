//! VYNX QR core — the QR engine with no UI dependency of any kind.
//!
//! Nothing in this crate knows about Qt, C++, JSON or any IPC. It owns the domain:
//! payload encoding, content detection, matrix construction, raster and vector
//! rendering, logo handling and scan verification. The presentation layer is a
//! caller.
//!
//! That boundary is what lets the engine be tested on its own and what keeps the
//! window layer replaceable.

pub mod detect;
pub mod error;
pub mod export;
pub mod formats;
pub mod platform;
pub mod qr;
pub mod settings;

pub use error::{AppError, AppResult, ErrorCode};
pub use qr::{render, QrMatrix, QrStyle, RenderRequest, RenderResult, ResolvedStyle};

/// Largest side length the renderer will produce, in pixels.
///
/// Anything bigger is refused rather than allocated: a QR code has no useful
/// detail above this size, and the allocation is attacker controlled input.
pub const MAX_RENDER_SIZE: u32 = 4096;

/// Largest logo accepted, in bytes, checked before the image is decoded.
pub const MAX_LOGO_BYTES: usize = 2 * 1024 * 1024;

/// Largest logo dimension accepted, in pixels, checked after decoding.
///
/// Without this a small compressed file could expand into gigabytes of RGBA.
pub const MAX_LOGO_DIMENSION: u32 = 4096;

/// Convenience constructor used by tests and the story of the crate.
pub fn request_for(text: &str) -> RenderRequest {
    RenderRequest {
        payload: qr::payload::QrPayload::Text {
            text: text.to_string(),
        },
        style: QrStyle::default(),
        ec_level: qr::payload::EcLevel::H,
        size_px: 512,
    }
}
