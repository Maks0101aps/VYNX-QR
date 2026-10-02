//! Geographic location (`geo:`) payloads.

use crate::error::{err_with, AppResult, ErrorCode};
use crate::formats::percent_encode;

/// Build a `geo:` URI as defined by RFC 5870.
///
/// `geo:<lat>,<lon>[,<alt>][;u=<uncertainty>][?q=<label>]`
pub fn encode(latitude: f64, longitude: f64, label: &str) -> AppResult<String> {
    if !latitude.is_finite() || !(-90.0..=90.0).contains(&latitude) {
        return Err(err_with(ErrorCode::InvalidLatitude, latitude.to_string()));
    }
    if !longitude.is_finite() || !(-180.0..=180.0).contains(&longitude) {
        return Err(err_with(ErrorCode::InvalidLongitude, longitude.to_string()));
    }

    let mut uri = format!(
        "geo:{},{}",
        format_coordinate(latitude),
        format_coordinate(longitude)
    );

    let label = label.trim();
    if !label.is_empty() {
        uri.push_str("?q=");
        uri.push_str(&percent_encode(label));
    }
    Ok(uri)
}

/// Six decimals is roughly 10 cm of precision, more than any phone GPS provides.
fn format_coordinate(value: f64) -> String {
    let rounded = (value * 1_000_000.0).round() / 1_000_000.0;
    let mut text = format!("{rounded:.6}");
    while text.ends_with('0') {
        text.pop();
    }
    if text.ends_with('.') {
        text.pop();
    }
    if text == "-0" {
        text = "0".to_string();
    }
    text
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn encodes_coordinates() {
        assert_eq!(encode(50.45, 30.52, "").expect("encode"), "geo:50.45,30.52");
        assert_eq!(encode(-33.8688, 151.2093, "").expect("encode"), "geo:-33.8688,151.2093");
    }

    #[test]
    fn trims_trailing_zeros() {
        assert_eq!(encode(0.0, 0.0, "").expect("encode"), "geo:0,0");
        assert_eq!(encode(1.5, -2.0, "").expect("encode"), "geo:1.5,-2");
    }

    #[test]
    fn appends_an_encoded_label() {
        assert_eq!(
            encode(50.45, 30.52, "Khreschatyk 1, Kyiv").expect("encode"),
            "geo:50.45,30.52?q=Khreschatyk%201%2C%20Kyiv"
        );
    }

    #[test]
    fn validates_ranges() {
        assert_eq!(encode(91.0, 0.0, "").unwrap_err().code(), ErrorCode::InvalidLatitude);
        assert_eq!(encode(-90.1, 0.0, "").unwrap_err().code(), ErrorCode::InvalidLatitude);
        assert_eq!(encode(0.0, 181.0, "").unwrap_err().code(), ErrorCode::InvalidLongitude);
        assert_eq!(encode(0.0, -180.1, "").unwrap_err().code(), ErrorCode::InvalidLongitude);
        assert_eq!(encode(f64::NAN, 0.0, "").unwrap_err().code(), ErrorCode::InvalidLatitude);
    }

    #[test]
    fn accepts_the_extremes() {
        assert!(encode(90.0, 180.0, "").is_ok());
        assert!(encode(-90.0, -180.0, "").is_ok());
    }
}
