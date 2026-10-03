//! Logo handling.
//!
//! The caller supplies the raw bytes of a raster image (PNG, JPEG or WebP).
//! Decoding happens once per render, which keeps this side free of any long lived
//! cache that would have to be invalidated.

use base64::engine::general_purpose::STANDARD as BASE64;
use base64::Engine;

use crate::error::{err_with, AppResult, ErrorCode};

/// Hard limit on the decoded image dimensions.
pub const MAX_LOGO_EDGE: u32 = crate::MAX_LOGO_DIMENSION;
/// Hard limit on the size of the encoded image.
pub const MAX_LOGO_BYTES: usize = crate::MAX_LOGO_BYTES;

/// Logo bytes as handed over by the caller.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct LogoInput {
    /// File name, shown in the UI.
    pub name: String,
    /// PNG, JPEG or WebP bytes.
    pub data: Vec<u8>,
}

/// A decoded logo, stored as straight RGBA8.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct LogoAsset {
    pub name: String,
    pub width: u32,
    pub height: u32,
    pub pixels: Vec<u8>,
}

impl LogoAsset {
    /// Decode and normalise to RGBA8.
    pub fn decode(input: &LogoInput) -> AppResult<Self> {
        Self::from_bytes(input.name.clone(), &input.data)
    }

    pub fn from_bytes(name: String, bytes: &[u8]) -> AppResult<Self> {
        if bytes.is_empty() {
            return Err(err_with(ErrorCode::LogoDecodeFailed, "empty file"));
        }
        if bytes.len() > MAX_LOGO_BYTES {
            return Err(err_with(
                ErrorCode::LogoTooBig,
                format!("{} bytes", bytes.len()),
            ));
        }
        let image = image::load_from_memory(bytes)
            .map_err(|error| err_with(ErrorCode::LogoDecodeFailed, error))?;
        let rgba = image.to_rgba8();
        let (width, height) = rgba.dimensions();
        if width == 0 || height == 0 {
            return Err(err_with(ErrorCode::LogoDecodeFailed, "zero sized image"));
        }
        if width > MAX_LOGO_EDGE || height > MAX_LOGO_EDGE {
            return Err(err_with(ErrorCode::LogoTooBig, format!("{width}x{height}")));
        }
        Ok(Self {
            name,
            width,
            height,
            pixels: rgba.into_raw(),
        })
    }

    #[inline]
    pub fn pixel(&self, x: u32, y: u32) -> crate::qr::Rgba {
        let index = ((y as usize) * (self.width as usize) + (x as usize)) * 4;
        if index + 3 >= self.pixels.len() {
            return crate::qr::Rgba::WHITE;
        }
        crate::qr::Rgba::new(
            self.pixels[index],
            self.pixels[index + 1],
            self.pixels[index + 2],
            self.pixels[index + 3],
        )
    }

    /// Scale to exactly `width` x `height` using a triangle filter, which keeps
    /// small logos readable without introducing ringing.
    pub fn scaled(&self, width: u32, height: u32) -> ScaledLogo {
        if width == self.width && height == self.height {
            return ScaledLogo {
                width,
                height,
                pixels: self.pixels.clone(),
            };
        }
        let source = image::RgbaImage::from_raw(self.width, self.height, self.pixels.clone())
            .unwrap_or_else(|| image::RgbaImage::new(self.width.max(1), self.height.max(1)));
        let resized = image::imageops::resize(
            &source,
            width,
            height,
            image::imageops::FilterType::Triangle,
        );
        ScaledLogo {
            width,
            height,
            pixels: resized.into_raw(),
        }
    }

    /// Re-encode as PNG so the SVG export can embed the logo losslessly.
    pub fn to_png_base64(&self) -> AppResult<String> {
        let mut out = Vec::new();
        let rgba = image::RgbaImage::from_raw(self.width, self.height, self.pixels.clone())
            .ok_or_else(|| err_with(ErrorCode::LogoDecodeFailed, "corrupt logo pixels"))?;
        image::DynamicImage::ImageRgba8(rgba)
            .write_to(&mut std::io::Cursor::new(&mut out), image::ImageFormat::Png)
            .map_err(|error| err_with(ErrorCode::LogoDecodeFailed, error))?;
        Ok(BASE64.encode(out))
    }
}

/// A logo scaled to a concrete pixel size.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ScaledLogo {
    pub width: u32,
    pub height: u32,
    pub pixels: Vec<u8>,
}

impl ScaledLogo {
    #[inline]
    pub fn pixel(&self, x: u32, y: u32) -> crate::qr::Rgba {
        let index = ((y as usize) * (self.width as usize) + (x as usize)) * 4;
        if index + 3 >= self.pixels.len() {
            return crate::qr::Rgba::WHITE;
        }
        crate::qr::Rgba::new(
            self.pixels[index],
            self.pixels[index + 1],
            self.pixels[index + 2],
            self.pixels[index + 3],
        )
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::qr::Rgba;

    fn tiny_png() -> Vec<u8> {
        let image = image::RgbaImage::from_pixel(4, 4, image::Rgba([10, 20, 30, 255]));
        let mut out = std::io::Cursor::new(Vec::new());
        image::DynamicImage::ImageRgba8(image)
            .write_to(&mut out, image::ImageFormat::Png)
            .expect("encode png");
        out.into_inner()
    }

    #[test]
    fn decodes_a_png() {
        let asset = LogoAsset::from_bytes("logo.png".into(), &tiny_png()).expect("decode");
        assert_eq!((asset.width, asset.height), (4, 4));
        assert_eq!(asset.pixel(0, 0), Rgba::new(10, 20, 30, 255));
    }

    #[test]
    fn rejects_garbage() {
        assert_eq!(
            LogoAsset::from_bytes("x.png".into(), b"not an image")
                .unwrap_err()
                .code(),
            ErrorCode::LogoDecodeFailed
        );
        assert_eq!(
            LogoAsset::from_bytes("x.png".into(), &[])
                .unwrap_err()
                .code(),
            ErrorCode::LogoDecodeFailed
        );
    }

    #[test]
    fn rejects_arbitrary_bytes() {
        let input = LogoInput {
            name: "x.png".into(),
            data: b"!!!!".to_vec(),
        };
        assert_eq!(
            LogoAsset::decode(&input).unwrap_err().code(),
            ErrorCode::LogoDecodeFailed
        );
    }

    #[test]
    fn rejects_empty_input() {
        let input = LogoInput {
            name: "x.png".into(),
            data: Vec::new(),
        };
        assert_eq!(
            LogoAsset::decode(&input).unwrap_err().code(),
            ErrorCode::LogoDecodeFailed
        );
    }

    #[test]
    fn scaling_preserves_dimensions() {
        let asset = LogoAsset::from_bytes("logo.png".into(), &tiny_png()).expect("decode");
        let scaled = asset.scaled(16, 8);
        assert_eq!((scaled.width, scaled.height), (16, 8));
        assert_eq!(scaled.pixels.len(), 16 * 8 * 4);
    }
}
