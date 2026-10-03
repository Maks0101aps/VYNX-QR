//! Phone (`tel:`) payloads.

use crate::error::{err_with, AppResult, ErrorCode};

/// Characters that are commonly used as visual separators inside phone numbers.
const SEPARATORS: [char; 6] = [' ', '-', '(', ')', '.', '\u{00a0}'];

/// Normalise a user supplied phone number.
///
/// Formatting characters are removed, an international `+` prefix is preserved
/// and nothing else is touched: digits are never rewritten, so an extension or a
/// premium rate prefix survives untouched.
pub fn normalize(input: &str) -> String {
    let trimmed = input.trim();
    let trimmed = trimmed
        .strip_prefix("tel:")
        .or_else(|| trimmed.strip_prefix("TEL:"))
        .unwrap_or(trimmed);
    let trimmed = trimmed.trim();
    if trimmed.starts_with('+') {
        let mut out = String::with_capacity(trimmed.len());
        out.push('+');
        for ch in trimmed.chars().skip(1) {
            if ch.is_ascii_digit() {
                out.push(ch);
            }
        }
        out
    } else {
        trimmed.chars().filter(|ch| ch.is_ascii_digit()).collect()
    }
}

/// True when the value plausibly is a phone number.
///
/// Deliberately conservative: a bare run of digits is *not* a phone number
/// unless it carries a `+` prefix, a `tel:` scheme or at least one formatting
/// separator. This keeps order numbers and prices out of the phone branch.
pub fn looks_like_phone(input: &str) -> bool {
    let trimmed = input.trim();
    let has_scheme = trimmed
        .get(..4)
        .is_some_and(|prefix| prefix.eq_ignore_ascii_case("tel:"))
        && trimmed[4..].chars().any(|c| c.is_ascii_digit());
    let body = trimmed
        .strip_prefix("tel:")
        .or_else(|| trimmed.strip_prefix("TEL:"))
        .unwrap_or(trimmed)
        .trim();

    if body.is_empty()
        || body
            .chars()
            .any(|c| !c.is_ascii_digit() && !SEPARATORS.contains(&c) && c != '+')
    {
        return false;
    }
    if body.chars().filter(|c| *c == '+').count() > 1 {
        return false;
    }
    let digits = body.chars().filter(|c| c.is_ascii_digit()).count();
    if !(7..=15).contains(&digits) {
        return false;
    }
    has_scheme || body.starts_with('+') || body.chars().any(|c| SEPARATORS.contains(&c))
}

/// Build a `tel:` URI.
pub fn encode(number: &str) -> AppResult<String> {
    if !looks_like_phone(number) {
        return Err(err_with(ErrorCode::InvalidPhone, number));
    }
    Ok(format!("tel:{}", normalize(number)))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn keeps_international_prefix() {
        assert_eq!(
            encode("+380991234567").expect("encode"),
            "tel:+380991234567"
        );
    }

    #[test]
    fn strips_formatting_characters() {
        assert_eq!(normalize("+1 (555) 010-9999"), "+15550109999");
        assert_eq!(normalize("  050 123 45 67 "), "0501234567");
    }

    #[test]
    fn strips_tel_scheme() {
        assert_eq!(
            encode("tel:+380991234567").expect("encode"),
            "tel:+380991234567"
        );
    }

    #[test]
    fn rejects_plain_digit_runs() {
        assert!(!looks_like_phone("123456"));
        assert!(!looks_like_phone("1234567890123456789"));
        assert!(!looks_like_phone(""));
    }

    #[test]
    fn accepts_separated_and_prefixed_numbers() {
        assert!(looks_like_phone("+380 99 123 4567"));
        assert!(looks_like_phone("(050) 123-45-67"));
        assert!(looks_like_phone("tel:0501234567"));
    }

    #[test]
    fn rejects_garbage() {
        assert_eq!(encode("hello").unwrap_err().code(), ErrorCode::InvalidPhone);
        assert_eq!(encode("+12").unwrap_err().code(), ErrorCode::InvalidPhone);
    }

    #[test]
    fn preserves_leading_plus_through_normalisation() {
        assert!(normalize("+380 (99) 123-45-67").starts_with('+'));
        assert!(!normalize("380991234567").starts_with('+'));
    }
}
