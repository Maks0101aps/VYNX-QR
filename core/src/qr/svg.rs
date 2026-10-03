//! True vector SVG export.
//!
//! Modules are emitted as real `<rect>` elements in module units, so the file
//! scales to any resolution without resampling and opens cleanly in a browser,
//! Inkscape or Illustrator. A logo, when present, is embedded as a base64 PNG
//! inside an SVG `<image>` element and clipped with an explicit plate.

use crate::error::AppResult;
use crate::qr::payload::ModuleStyle;
use crate::qr::{QrMatrix, ResolvedStyle};

/// Build the SVG document for a rendered QR code.
///
/// `size_px` sets the intrinsic `width`/`height` attributes; the `viewBox` is in
/// module units so the file remains resolution independent.
pub fn export(matrix: &QrMatrix, style: &ResolvedStyle, size_px: u32) -> AppResult<String> {
    let quiet = style.quiet_zone as usize;
    let total = matrix.size + 2 * quiet;
    let size_px = size_px.max(total as u32);

    let mut svg = String::with_capacity(2048 + matrix.size * matrix.size * 12);
    svg.push_str(&format!(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\
         <svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\" \
         width=\"{size_px}\" height=\"{size_px}\" viewBox=\"0 0 {total} {total}\" \
         role=\"img\" aria-label=\"QR code\">\n"
    ));
    svg.push_str(&format!(
        "  <title>VYNX QR</title>\n  <desc>{} module QR code generated locally by VYNX QR.</desc>\n",
        matrix.size
    ));

    // Background, including the quiet zone.
    svg.push_str(&format!(
        "  <rect x=\"0\" y=\"0\" width=\"{total}\" height=\"{total}\" fill=\"{}\"/>\n",
        style.background.to_hex()
    ));

    // Optional logo plate: a square of background colour under the logo.
    let logo_box = match &style.logo {
        Some(_) => {
            let modules = ((matrix.size as f32) * style.logo_ratio).round() as usize;
            let modules = modules.clamp(3, (matrix.size / 3).max(3));
            let offset = (matrix.size - modules) / 2 + quiet;
            Some((offset, modules))
        }
        None => None,
    };

    // Dark modules.
    svg.push_str(&format!(
        "  <g fill=\"{}\" shape-rendering=\"{}\">\n",
        style.foreground.to_hex(),
        if style.module_style == ModuleStyle::Square {
            "crispEdges"
        } else {
            "geometricPrecision"
        }
    ));
    match style.module_style {
        ModuleStyle::Square => write_merged_runs(matrix, quiet, &mut svg),
        ModuleStyle::Rounded => write_rounded_modules(matrix, quiet, &mut svg),
    }
    if style.module_style == ModuleStyle::Rounded {
        write_rounded_finders(matrix, quiet, &style.background.to_hex(), &mut svg);
    }
    svg.push_str("  </g>\n");

    // Punch the plate out of the module layer by overlaying background coloured
    // rectangles: cheaper and more portable than a clip path.
    if let (Some((offset, modules)), Some(_)) = (logo_box, &style.logo) {
        svg.push_str(&format!(
            "  <g fill=\"{}\">\n    <rect x=\"{}\" y=\"{}\" width=\"{modules}\" height=\"{modules}\"/>\n  </g>\n",
            style.background.to_hex(),
            offset,
            offset
        ));
    }

    if let (Some((offset, modules)), Some(logo)) = (logo_box, &style.logo) {
        let inner = ((modules as f32) * 0.86).round() as u32;
        if inner > 0 {
            let image_w = logo.width.max(1);
            let image_h = logo.height.max(1);
            let scale = (inner as f32 / image_w as f32).min(inner as f32 / image_h as f32);
            let draw_w = (image_w as f32 * scale).round().max(1.0);
            let draw_h = (image_h as f32 * scale).round().max(1.0);
            let x = offset as f32 + (modules as f32 - draw_w) / 2.0;
            let y = offset as f32 + (modules as f32 - draw_h) / 2.0;
            let href = logo.to_png_base64()?;
            svg.push_str(&format!(
                "  <image x=\"{x:.3}\" y=\"{y:.3}\" width=\"{draw_w:.3}\" height=\"{draw_h:.3}\" \
                 preserveAspectRatio=\"xMidYMid meet\" href=\"data:image/png;base64,{href}\" \
                 xlink:href=\"data:image/png;base64,{href}\"/>\n"
            ));
        }
    }

    svg.push_str("</svg>\n");
    Ok(svg)
}

/// Square style: merge horizontal runs of dark modules into single rectangles so
/// the file stays small and the edges stay perfectly crisp.
fn write_merged_runs(matrix: &QrMatrix, quiet: usize, svg: &mut String) {
    for y in 0..matrix.size {
        let mut x = 0usize;
        while x < matrix.size {
            if !matrix.get(x, y) {
                x += 1;
                continue;
            }
            let start = x;
            while x < matrix.size && matrix.get(x, y) {
                x += 1;
            }
            let width = x - start;
            svg.push_str(&format!(
                "    <rect x=\"{}\" y=\"{}\" width=\"{width}\" height=\"1\"/>\n",
                start + quiet,
                y + quiet
            ));
        }
    }
}

/// Rounded style: one rounded rectangle per dark data module.
fn write_rounded_modules(matrix: &QrMatrix, quiet: usize, svg: &mut String) {
    for y in 0..matrix.size {
        for x in 0..matrix.size {
            if !matrix.get(x, y) || matrix.is_finder(x, y) {
                continue;
            }
            svg.push_str(&format!(
                "    <rect x=\"{}\" y=\"{}\" width=\"1\" height=\"1\" rx=\"0.28\"/>\n",
                x + quiet,
                y + quiet
            ));
        }
    }
}

/// Rounded style: the three position detection patterns, drawn as concentric
/// rounded squares so they stay unmistakably finder patterns.
fn write_rounded_finders(matrix: &QrMatrix, quiet: usize, background: &str, svg: &mut String) {
    let far = matrix.size.saturating_sub(7);
    for (ox, oy) in [(0usize, 0usize), (far, 0usize), (0usize, far)] {
        svg.push_str(&format!(
            "    <rect x=\"{}\" y=\"{}\" width=\"7\" height=\"7\" rx=\"0.9\"/>\n",
            ox + quiet,
            oy + quiet
        ));
        svg.push_str(&format!(
            "    <rect x=\"{}\" y=\"{}\" width=\"5\" height=\"5\" rx=\"0.7\" fill=\"{background}\"/>\n",
            ox + quiet + 1,
            oy + quiet + 1
        ));
        svg.push_str(&format!(
            "    <rect x=\"{}\" y=\"{}\" width=\"3\" height=\"3\" rx=\"0.5\"/>\n",
            ox + quiet + 2,
            oy + quiet + 2
        ));
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::qr::payload::{EcLevel, ModuleStyle};
    use crate::qr::{build_matrix, QrStyle};

    fn svg_for(style: QrStyle) -> String {
        let matrix = build_matrix("https://github.com/VYNX", EcLevel::M).expect("matrix");
        let resolved = style.resolve().expect("style");
        export(&matrix, &resolved, 1024).expect("svg")
    }

    #[test]
    fn emits_valid_svg_shell() {
        let document = svg_for(QrStyle::default());
        assert!(document.starts_with("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"));
        assert!(document.contains("<svg xmlns=\"http://www.w3.org/2000/svg\""));
        assert!(document.trim_end().ends_with("</svg>"));
        // `https://github.com/VYNX` at EC level M is version 2, so 25 modules plus a
        // quiet zone of 4 on every side.
        assert!(document.contains("viewBox=\"0 0 33 33\""));
    }

    #[test]
    fn square_style_uses_merged_rectangles() {
        let document = svg_for(QrStyle::default());
        assert!(document.contains("shape-rendering=\"crispEdges\""));
        assert!(document.contains("<rect x=\"4\" y=\"4\" width=\"7\" height=\"1\"/>"));
    }

    #[test]
    fn rounded_style_uses_rounded_rectangles() {
        let style = QrStyle {
            module_style: ModuleStyle::Rounded,
            ..QrStyle::default()
        };
        let document = svg_for(style);
        assert!(document.contains("rx=\"0.28\""));
        assert!(document.contains("rx=\"0.9\""));
        assert!(document.contains("fill=\"#FFFFFF\""));
    }

    #[test]
    fn never_embeds_a_raster_qr() {
        let document = svg_for(QrStyle::default());
        assert!(!document.contains("data:image/png;base64"));
    }

    #[test]
    fn background_is_drawn() {
        let document = svg_for(QrStyle::default());
        assert!(document.contains("fill=\"#FFFFFF\""));
    }
}
