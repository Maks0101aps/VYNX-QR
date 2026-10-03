//! Email (`mailto:`) payloads.

use crate::error::{err_with, AppResult, ErrorCode};
use crate::formats::percent_encode;

/// Build a `mailto:` URI.
///
/// Addresses may contain several recipients separated by `,`, which is legal in
/// a `mailto:` URI and understood by every mail client.
pub fn encode(to: &str, subject: &str, body: &str) -> AppResult<String> {
    let parts: Vec<&str> = to.split(',').map(str::trim).collect();
    if parts.iter().all(|part| part.is_empty()) {
        return Err(err_with(ErrorCode::InvalidEmail, "recipient is blank"));
    }
    for recipient in &parts {
        // Whitespace is only tolerated as the separator padding, never inside an
        // address, and it must not survive into the emitted URI.
        if recipient.chars().any(char::is_whitespace) || !crate::detect::looks_like_email(recipient)
        {
            return Err(err_with(ErrorCode::InvalidEmail, recipient));
        }
    }
    let recipients = parts.join(",");

    let mut uri = String::with_capacity(32 + recipients.len() + subject.len() + body.len());
    uri.push_str("mailto:");
    uri.push_str(&recipients);
    let subject = subject.trim();
    let body = body.trim();
    if !subject.is_empty() || !body.is_empty() {
        uri.push('?');
        if !subject.is_empty() {
            uri.push_str(&format!("subject={}", percent_encode(subject)));
        }
        if !subject.is_empty() && !body.is_empty() {
            uri.push('&');
        }
        if !body.is_empty() {
            uri.push_str(&format!("body={}", percent_encode(body)));
        }
    }
    Ok(uri)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn bare_recipient() {
        assert_eq!(
            encode("hi@example.com", "", "").expect("encode"),
            "mailto:hi@example.com"
        );
    }

    #[test]
    fn subject_and_body_are_percent_encoded() {
        let payload =
            encode("hi@example.com", "Привіт & hello", "Line one\nLine two").expect("encode");
        assert_eq!(
            payload,
            "mailto:hi@example.com?subject=%D0%9F%D1%80%D0%B8%D0%B2%D1%96%D1%82%20%26%20hello&body=Line%20one%0ALine%20two"
        );
    }

    #[test]
    fn supports_multiple_recipients() {
        let payload = encode("a@x.dev, b@x.dev", "Hi", "").expect("encode");
        assert_eq!(payload, "mailto:a@x.dev,b@x.dev?subject=Hi");
    }

    #[test]
    fn rejects_invalid_input() {
        assert_eq!(
            encode("", "", "").unwrap_err().code(),
            ErrorCode::InvalidEmail
        );
        assert_eq!(
            encode("nope", "", "").unwrap_err().code(),
            ErrorCode::InvalidEmail
        );
        assert_eq!(
            encode("a b@x.dev", "", "").unwrap_err().code(),
            ErrorCode::InvalidEmail
        );
    }
}
