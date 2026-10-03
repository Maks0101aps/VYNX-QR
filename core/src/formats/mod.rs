//! Payload encoders.
//!
//! Every encoder is a pure function: input in, QR string out. They never touch
//! the file system, the network or global state, which makes them trivial to
//! unit test and keeps the QR pipeline free of side effects.

pub mod email;
pub mod geo;
pub mod phone;
pub mod sms;
pub mod text;
pub mod url;
pub mod vcard;
pub mod wifi;

/// Percent-encode a string for use inside a `mailto:` / `geo:` query component.
///
/// Unreserved characters (`A-Z a-z 0-9 - _ . ! ~ * ' ( )`) are kept, everything
/// else becomes uppercase percent-encoded UTF-8 bytes. Spaces become `%20`
/// rather than `+` because QR scanners read the payload literally.
pub fn percent_encode(input: &str) -> String {
    const UNRESERVED: &[u8] = b"-_.!~*'()";
    let mut out = String::with_capacity(input.len());
    for byte in input.as_bytes() {
        if byte.is_ascii_alphanumeric() || UNRESERVED.contains(byte) {
            out.push(*byte as char);
        } else {
            out.push('%');
            out.push_str(&format!("{byte:02X}"));
        }
    }
    out
}

/// Escape a value for the `WIFI:` payload format.
///
/// The format is a `key:value` list separated by `;`, so `\`, `;`, `,`, `:` and
/// the double quotes have to be backslash escaped.
pub fn escape_wifi_value(value: &str) -> String {
    let mut out = String::with_capacity(value.len() + 4);
    for ch in value.chars() {
        match ch {
            '\\' | ';' | ',' | ':' | '"' => {
                out.push('\\');
                out.push(ch);
            }
            _ => out.push(ch),
        }
    }
    out
}

/// Collapse Windows-style whitespace and trim.
pub fn trim(value: &str) -> &str {
    value.trim()
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn percent_encoding_keeps_unreserved_characters() {
        assert_eq!(percent_encode("abcXYZ019-_.!~*'()"), "abcXYZ019-_.!~*'()");
    }

    #[test]
    fn percent_encoding_escapes_spaces_and_unicode() {
        assert_eq!(percent_encode("a b"), "a%20b");
        assert_eq!(
            percent_encode("Привіт"),
            "%D0%9F%D1%80%D0%B8%D0%B2%D1%96%D1%82"
        );
        assert_eq!(percent_encode("🇺🇦"), "%F0%9F%87%BA%F0%9F%87%A6");
    }

    #[test]
    fn percent_encoding_escapes_query_separators() {
        assert_eq!(percent_encode("a&b=c"), "a%26b%3Dc");
        assert_eq!(percent_encode("q?#"), "q%3F%23");
    }

    #[test]
    fn wifi_escaping_covers_special_characters() {
        assert_eq!(
            escape_wifi_value(r#"My;Password:123"#),
            r#"My\;Password\:123"#
        );
        assert_eq!(escape_wifi_value(r#"back\slash"#), r#"back\\slash"#);
        assert_eq!(escape_wifi_value(r#"a,b"c"#), r#"a\,b\"c"#);
        assert_eq!(escape_wifi_value("VYNX Home"), "VYNX Home");
    }
}
