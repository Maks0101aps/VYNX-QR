//! Raster rendering of a QR matrix.
//!
//! Modules are always drawn on an integer pixel grid: every module becomes
//! exactly `cell x cell` pixels, so no module is ever half a pixel wide and the
//! result stays perfectly crisp. Any leftover pixels are filled with the
//! background colour, which is invisible because it sits inside the quiet zone.

use image::codecs::png::{CompressionType, FilterType as PngFilter, PngEncoder};
use image::{ExtendedColorType, ImageEncoder};

use crate::error::{err_with, AppResult, ErrorCode};
use crate::qr::logo::LogoAsset;
use crate::qr::payload::ModuleStyle;
use crate::qr::{QrMatrix, ResolvedStyle};

/// Straight (non premultiplied) 8 bit RGBA colour.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Rgba {
    pub r: u8,
    pub g: u8,
    pub b: u8,
    pub a: u8,
}

impl Rgba {
    pub const fn new(r: u8, g: u8, b: u8, a: u8) -> Self {
        Self { r, g, b, a }
    }

    pub const BLACK: Self = Self::new(0, 0, 0, 255);
    pub const WHITE: Self = Self::new(255, 255, 255, 255);

    /// Parse `#RGB`, `#RRGGBB` or `#RRGGBBAA` (the leading `#` is optional).
    pub fn from_hex(value: &str) -> AppResult<Self> {
        let hex = value.trim().trim_start_matches('#');
        let valid = matches!(hex.len(), 3 | 6 | 8) && hex.chars().all(|c| c.is_ascii_hexdigit());
        if !valid {
            return Err(err_with(ErrorCode::InvalidColor, value));
        }
        let expanded = if hex.len() == 3 {
            hex.chars().flat_map(|c| [c, c]).collect::<String>()
        } else {
            hex.to_string()
        };
        let byte = |index: usize| -> u8 {
            u8::from_str_radix(&expanded[index..index + 2], 16).unwrap_or(0)
        };
        Ok(match expanded.len() {
            6 => Self::new(byte(0), byte(2), byte(4), 255),
            _ => Self::new(byte(0), byte(2), byte(4), byte(6)),
        })
    }

    pub fn to_hex(self) -> String {
        if self.a == 255 {
            format!("#{:02X}{:02X}{:02X}", self.r, self.g, self.b)
        } else {
            format!("#{:02X}{:02X}{:02X}{:02X}", self.r, self.g, self.b, self.a)
        }
    }

    /// WCAG relative luminance.
    pub fn relative_luminance(self) -> f32 {
        let channel = |value: u8| -> f32 {
            let value = f32::from(value) / 255.0;
            if value <= 0.040_45 {
                value / 12.92
            } else {
                ((value + 0.055) / 1.055).powf(2.4)
            }
        };
        0.2126 * channel(self.r) + 0.7152 * channel(self.g) + 0.0722 * channel(self.b)
    }

    /// Composite `self` over `dst` using straight alpha.
    pub fn over(self, dst: Rgba) -> Rgba {
        if self.a == 255 {
            return self;
        }
        if self.a == 0 {
            return dst;
        }
        let sa = f32::from(self.a) / 255.0;
        let da = f32::from(dst.a) / 255.0;
        let out_a = sa + da * (1.0 - sa);
        if out_a <= 0.0 {
            return Rgba::new(0, 0, 0, 0);
        }
        let mix = |s: u8, d: u8| -> u8 {
            let value = (f32::from(s) * sa + f32::from(d) * da * (1.0 - sa)) / out_a;
            value.clamp(0.0, 255.0).round() as u8
        };
        Rgba::new(mix(self.r, dst.r), mix(self.g, dst.g), mix(self.b, dst.b), (out_a * 255.0).round() as u8)
    }
}

/// A rendered RGBA image.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Canvas {
    pub width: u32,
    pub height: u32,
    /// RGBA8, row major.
    pub pixels: Vec<u8>,
}

impl Canvas {
    pub fn new(width: u32, height: u32, fill: Rgba) -> Self {
        let mut pixels = Vec::with_capacity((width as usize) * (height as usize) * 4);
        for _ in 0..(width as usize) * (height as usize) {
            pixels.extend_from_slice(&[fill.r, fill.g, fill.b, fill.a]);
        }
        Self { width, height, pixels }
    }

    #[inline]
    pub fn set(&mut self, x: u32, y: u32, color: Rgba) {
        if x >= self.width || y >= self.height {
            return;
        }
        let index = ((y as usize) * (self.width as usize) + (x as usize)) * 4;
        if color.a == 255 {
            self.pixels[index] = color.r;
            self.pixels[index + 1] = color.g;
            self.pixels[index + 2] = color.b;
            self.pixels[index + 3] = 255;
            return;
        }
        let existing = Rgba::new(
            self.pixels[index],
            self.pixels[index + 1],
            self.pixels[index + 2],
            self.pixels[index + 3],
        );
        let blended = color.over(existing);
        self.pixels[index] = blended.r;
        self.pixels[index + 1] = blended.g;
        self.pixels[index + 2] = blended.b;
        self.pixels[index + 3] = blended.a;
    }

    /// Convert to 8 bit greyscale using Rec. 601 luma, which is what QR decoders
    /// effectively see after binarisation.
    pub fn to_greyscale(&self) -> Vec<u8> {
        let mut out = Vec::with_capacity((self.width as usize) * (self.height as usize));
        for chunk in self.pixels.chunks_exact(4) {
            let luma = 0.299 * f32::from(chunk[0]) + 0.587 * f32::from(chunk[1]) + 0.114 * f32::from(chunk[2]);
            out.push(luma.round().clamp(0.0, 255.0) as u8);
        }
        out
    }
}

/// Render `matrix` with `style` into a square canvas of `size_px` pixels.
pub fn rasterize(matrix: &QrMatrix, style: &ResolvedStyle, size_px: u32) -> Canvas {
    let total = matrix.size as u32 + 2 * style.quiet_zone;
    let cell = (size_px / total.max(1)).max(1);
    let drawn = cell * total;
    let padding = (size_px.saturating_sub(drawn)) / 2;

    let mut canvas = Canvas::new(size_px, size_px, style.background);

    for y in 0..matrix.size {
        for x in 0..matrix.size {
            if !matrix.get(x, y) {
                continue;
            }
            let px = padding + (x as u32 + style.quiet_zone) * cell;
            let py = padding + (y as u32 + style.quiet_zone) * cell;
            match style.module_style {
                ModuleStyle::Square => fill_rect(&mut canvas, px, py, cell, cell, style.foreground),
                ModuleStyle::Rounded => {
                    fill_rounded_rect(&mut canvas, px, py, cell, cell, style.foreground);
                }
            }
        }
    }

    if style.module_style == ModuleStyle::Rounded {
        draw_finders(matrix, style, cell, padding, &mut canvas);
    }

    if let Some(logo) = &style.logo {
        draw_logo(matrix, style, cell, padding, logo, &mut canvas);
    }

    canvas
}

fn draw_finders(matrix: &QrMatrix, style: &ResolvedStyle, cell: u32, padding: u32, canvas: &mut Canvas) {
    let n = matrix.size as u32;
    let far = n.saturating_sub(7);
    let origins = [(0u32, 0u32), (far, 0u32), (0u32, far)];
    for (ox, oy) in origins {
        let x = padding + (ox + style.quiet_zone) * cell;
        let y = padding + (oy + style.quiet_zone) * cell;
        let side = 7 * cell;
        // Outer ring, inner gap and centre dot keep the position detection pattern
        // clearly readable even with rounded corners everywhere else.
        fill_rounded_rect(canvas, x, y, side, side, style.foreground);
        let inset = cell;
        fill_rounded_rect(canvas, x + inset, y + inset, side - 2 * inset, side - 2 * inset, style.background);
        let inset = 2 * cell;
        fill_rounded_rect(canvas, x + inset, y + inset, 3 * cell, 3 * cell, style.foreground);
    }
}

fn draw_logo(
    matrix: &QrMatrix,
    style: &ResolvedStyle,
    cell: u32,
    padding: u32,
    logo: &LogoAsset,
    canvas: &mut Canvas,
) {
    let total = matrix.size as f32;
    let logo_modules = ((total * style.logo_ratio).round() as u32).clamp(3, matrix.size as u32 / 3);
    let box_px = logo_modules * cell;
    let offset_modules = matrix.size as u32 - logo_modules;
    let offset_modules = offset_modules / 2;
    let x = padding + (offset_modules + style.quiet_zone) * cell;
    let y = x;

    // Clear a plate so the logo never sits on top of unrelated dark modules.
    fill_rect(canvas, x, y, box_px, box_px, style.background);

    let inner = ((box_px as f32) * 0.86).round() as u32;
    if inner < 4 {
        return;
    }
    let image_w = logo.width.max(1);
    let image_h = logo.height.max(1);
    let scale = (inner as f32 / image_w as f32).min(inner as f32 / image_h as f32);
    let draw_w = ((image_w as f32) * scale).round().max(1.0) as u32;
    let draw_h = ((image_h as f32) * scale).round().max(1.0) as u32;
    let draw_x = x + (box_px - draw_w) / 2;
    let draw_y = y + (box_px - draw_h) / 2;

    let target = logo.scaled(draw_w, draw_h);
    for row in 0..draw_h {
        for column in 0..draw_w {
            let color = target.pixel(column, row);
            canvas.set(draw_x + column, draw_y + row, color);
        }
    }
}

fn fill_rect(canvas: &mut Canvas, x: u32, y: u32, width: u32, height: u32, color: Rgba) {
    for row in y..(y + height) {
        for column in x..(x + width) {
            canvas.set(column, row, color);
        }
    }
}

/// Fill a rounded rectangle. `radius` is a quarter of the smaller side, which
/// keeps the result recognisable at every module size.
fn fill_rounded_rect(canvas: &mut Canvas, x: u32, y: u32, width: u32, height: u32, color: Rgba) {
    let radius = (width.min(height) as f32 * 0.28).round() as u32;
    if radius == 0 {
        fill_rect(canvas, x, y, width, height, color);
        return;
    }
    let radius = radius.min(width / 2).min(height / 2);

    for row in y..(y + height) {
        for column in x..(x + width) {
            if inside_rounded(column, row, x, y, width, height, radius) {
                canvas.set(column, row, color);
            }
        }
    }
}

fn inside_rounded(px: u32, py: u32, x: u32, y: u32, width: u32, height: u32, radius: u32) -> bool {
    let left = x as i64;
    let top = y as i64;
    let right = (x + width) as i64 - 1;
    let bottom = (y + height) as i64 - 1;
    let px = px as i64;
    let py = py as i64;

    let corner_x = if px < left + radius as i64 {
        left + radius as i64
    } else if px > right - radius as i64 {
        right - radius as i64
    } else {
        px
    };
    let corner_y = if py < top + radius as i64 {
        top + radius as i64
    } else if py > bottom - radius as i64 {
        bottom - radius as i64
    } else {
        py
    };

    if px == corner_x && py == corner_y {
        return true;
    }
    let dx = px - corner_x;
    let dy = py - corner_y;
    dx * dx + dy * dy <= (radius * radius) as i64
}

/// Encode a canvas as PNG bytes.
///
/// Raw bytes, not a data URL. The window builds a `QImage` straight from this
/// buffer, so there is nothing to base64 encode or decode anywhere on the path.
pub fn encode_png(canvas: &Canvas) -> AppResult<Vec<u8>> {
    let mut out = Vec::new();
    PngEncoder::new_with_quality(&mut out, CompressionType::Default, PngFilter::Adaptive)
        .write_image(&canvas.pixels, canvas.width, canvas.height, ExtendedColorType::Rgba8)
        .map_err(|error| err_with(ErrorCode::RenderFailed, error))?;
    Ok(out)
}

/// Used by clipboard export, which needs BGRA on Windows.
pub fn canvas_to_bgra(canvas: &Canvas) -> Vec<u8> {
    let mut out = Vec::with_capacity(canvas.pixels.len());
    for chunk in canvas.pixels.chunks_exact(4) {
        out.extend_from_slice(&[chunk[2], chunk[1], chunk[0], chunk[3]]);
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::qr::payload::EcLevel;
    use crate::qr::{build_matrix, QrStyle};

    #[test]
    fn parses_hex_colours() {
        assert_eq!(Rgba::from_hex("#000000").expect("black"), Rgba::BLACK);
        assert_eq!(Rgba::from_hex("ffffff").expect("white"), Rgba::WHITE);
        assert_eq!(Rgba::from_hex("#FFF").expect("short"), Rgba::WHITE);
        assert_eq!(Rgba::from_hex("#102030").expect("full"), Rgba::new(0x10, 0x20, 0x30, 255));
        assert!(Rgba::from_hex("nope").is_err());
        assert!(Rgba::from_hex("#12345").is_err());
    }

    #[test]
    fn renders_at_the_requested_size() {
        let matrix = build_matrix("https://github.com", EcLevel::M).expect("matrix");
        let style = QrStyle::default().resolve().expect("style");
        let canvas = rasterize(&matrix, &style, 1024);
        assert_eq!((canvas.width, canvas.height), (1024, 1024));
        assert_eq!(canvas.pixels.len(), 1024 * 1024 * 4);
    }

    #[test]
    fn quiet_zone_is_background() {
        let matrix = build_matrix("VYNX", EcLevel::M).expect("matrix");
        let style = QrStyle::default().resolve().expect("style");
        let canvas = rasterize(&matrix, &style, 512);
        let corner = &canvas.pixels[0..4];
        assert_eq!(corner, &[255, 255, 255, 255]);
    }

    #[test]
    fn greyscale_conversion_keeps_black_and_white() {
        let mut canvas = Canvas::new(2, 1, Rgba::BLACK);
        canvas.set(1, 0, Rgba::WHITE);
        assert_eq!(canvas.to_greyscale(), vec![0, 255]);
    }

    #[test]
    fn alpha_compositing_moves_towards_the_source() {
        let result = Rgba::new(0, 0, 0, 128).over(Rgba::WHITE);
        assert!(result.r > 100 && result.r < 160);
        assert_eq!(result.a, 255);
    }
}
