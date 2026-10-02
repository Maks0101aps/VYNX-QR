//! Plain text payloads.

use crate::error::{err_with, AppResult, ErrorCode};

/// QR codes can carry arbitrary text; the only hard limits are emptiness and the
/// symbol capacity, which the encoder checks later.
pub fn encode(text: &str) -> AppResult<String> {
    if text.trim().is_empty() {
        return Err(err_with(ErrorCode::EmptyInput, "text payload is blank"));
    }
    Ok(text.to_string())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn keeps_unicode_and_whitespace_verbatim() {
        let input = "Привіт, Україно 🇺🇦";
        assert_eq!(encode(input).expect("encode"), input);
    }

    #[test]
    fn rejects_blank_text() {
        assert_eq!(encode("   \n ").unwrap_err().code(), ErrorCode::EmptyInput);
    }
}
