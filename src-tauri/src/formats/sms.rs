//! SMS (`SMSTO:`) payloads.

use crate::error::{err_with, AppResult, ErrorCode};

/// Build an `SMSTO:` URI.
///
/// `SMSTO:<number>:<message>` is the spelling that the widest range of phone
/// cameras and QR scanner apps recognise, so it is preferred over the newer
/// `sms:` RFC 5724 form.
pub fn encode(number: &str, message: &str) -> AppResult<String> {
    if !crate::formats::phone::looks_like_phone(number) {
        return Err(err_with(ErrorCode::InvalidPhone, number));
    }
    let recipient = crate::formats::phone::normalize(number);
    let message = message.trim();
    // Colons inside the message would be read as a separator by older scanners.
    let message = message.replace(':', " ");
    Ok(format!("SMSTO:{recipient}:{message}"))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn encodes_number_and_message() {
        assert_eq!(encode("+380991234567", "Hi there").expect("encode"), "SMSTO:+380991234567:Hi there");
    }

    #[test]
    fn allows_an_empty_message() {
        assert_eq!(encode("+380991234567", "").expect("encode"), "SMSTO:+380991234567:");
    }

    #[test]
    fn keeps_unicode_messages() {
        assert_eq!(
            encode("+380991234567", "Привіт 🇺🇦").expect("encode"),
            "SMSTO:+380991234567:Привіт 🇺🇦"
        );
    }

    #[test]
    fn neutralises_colons_in_the_message() {
        assert_eq!(encode("+380991234567", "a:b").expect("encode"), "SMSTO:+380991234567:a b");
    }

    #[test]
    fn rejects_invalid_number() {
        assert_eq!(encode("nope", "hi").unwrap_err().code(), ErrorCode::InvalidPhone);
    }
}
