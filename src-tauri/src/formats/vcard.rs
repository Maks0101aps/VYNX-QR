//! vCard 3.0 contact payloads.
//!
//! vCard 3.0 is used because it is the version understood by the largest number
//! of phone camera scanners and by Outlook, Google Contacts and the Apple
//! address book alike. Values are escaped per RFC 2426 §5 and lines are folded
//! with CRLF, which is what every scanner expects.

use crate::error::{err_with, AppResult, ErrorCode};

/// Escape a vCard value: backslash, comma, semicolon and newlines.
pub fn escape_value(value: &str) -> String {
    let mut out = String::with_capacity(value.len() + 8);
    for ch in value.chars() {
        match ch {
            '\\' => out.push_str("\\\\"),
            ';' => out.push_str("\\;"),
            ',' => out.push_str("\\,"),
            '\n' => out.push_str("\\n"),
            '\r' => {}
            other => out.push(other),
        }
    }
    out
}

#[allow(clippy::too_many_arguments)]
pub fn encode(
    first_name: &str,
    last_name: &str,
    organization: &str,
    job_title: &str,
    phone: &str,
    email: &str,
    website: &str,
    address: &str,
    note: &str,
) -> AppResult<String> {
    if [first_name, last_name, organization, job_title, phone, email, website, address, note]
        .iter()
        .all(|field| field.trim().is_empty())
    {
        return Err(err_with(ErrorCode::EmptyInput, "vCard has no content"));
    }

    if !email.trim().is_empty() && !crate::detect::looks_like_email(email) {
        return Err(err_with(ErrorCode::InvalidEmail, email));
    }

    let full_name = format!("{} {}", first_name.trim(), last_name.trim());
    let full_name = full_name.trim().to_string();

    let mut lines: Vec<String> = vec!["BEGIN:VCARD".into(), "VERSION:3.0".into()];
    if !full_name.is_empty() {
        lines.push("N:".to_string() + &escape_value(last_name) + ";" + &escape_value(first_name) + ";;;");
        lines.push(format!("FN:{}", escape_value(&full_name)));
    }
    push_if_present(&mut lines, "ORG", organization);
    push_if_present(&mut lines, "TITLE", job_title);
    push_if_present(&mut lines, "TEL;TYPE=CELL", phone);
    push_if_present(&mut lines, "EMAIL;TYPE=INTERNET", email);
    if !website.trim().is_empty() {
        // Web addresses inside vCards need a scheme to be tappable.
        let site = website.trim();
        let site = if site.starts_with("http://") || site.starts_with("https://") {
            site.to_string()
        } else {
            format!("https://{site}")
        };
        lines.push(format!("URL:{}", escape_value(&site)));
    }
    push_if_present(&mut lines, "ADR;TYPE=HOME", address);
    push_if_present(&mut lines, "NOTE", note);
    lines.push("END:VCARD".into());

    Ok(lines.join("\r\n"))
}

fn push_if_present(lines: &mut Vec<String>, key: &str, value: &str) {
    let trimmed = value.trim();
    if trimmed.is_empty() {
        return;
    }
    lines.push(format!("{key}:{}", escape_value(trimmed)));
}

#[cfg(test)]
mod tests {
    use super::*;

    fn full() -> AppResult<String> {
        encode(
            "Олена",
            "Ковальчук",
            "VYNX",
            "Engineer",
            "+380991234567",
            "olena@example.com",
            "vynx.dev",
            "Kyiv, Ukraine",
            "Met at VYNX ✨",
        )
    }

    #[test]
    fn produces_a_well_formed_vcard() {
        let payload = full().expect("encode");
        assert!(payload.starts_with("BEGIN:VCARD\r\nVERSION:3.0\r\n"));
        assert!(payload.ends_with("\r\nEND:VCARD"));
        assert!(payload.contains("N:Ковальчук;Олена;;;"));
        assert!(payload.contains("FN:Олена Ковальчук"));
        assert!(payload.contains("ORG:VYNX"));
        assert!(payload.contains("TEL;TYPE=CELL:+380991234567"));
        assert!(payload.contains("EMAIL;TYPE=INTERNET:olena@example.com"));
        assert!(payload.contains("URL:https://vynx.dev"));
        assert!(payload.contains(r"ADR;TYPE=HOME:Kyiv\, Ukraine"));
    }

    #[test]
    fn escapes_delimiters_and_newlines() {
        let payload = encode("A", "B", "", "", "", "", "", "", "line1\nline2; semi, comma").expect("encode");
        assert!(payload.contains("NOTE:line1\\nline2\\; semi\\, comma"));
    }

    #[test]
    fn keeps_emoji_and_non_latin_scripts() {
        let payload = encode("Taro", "山田", "", "", "", "", "", "", "こんにちは 🇯🇵").expect("encode");
        assert!(payload.contains("FN:Taro 山田"));
        assert!(payload.contains("NOTE:こんにちは 🇯🇵"));
    }

    #[test]
    fn omits_blank_fields() {
        let payload = encode("Ada", "", "", "", "", "", "", "", "").expect("encode");
        assert!(payload.contains("FN:Ada"));
        assert!(!payload.contains("ORG:"));
        assert!(!payload.contains("TITLE:"));
        assert!(!payload.contains("URL:"));
    }

    #[test]
    fn rejects_empty_contact() {
        assert_eq!(encode("", "", "", "", "", "", "", "", "").unwrap_err().code(), ErrorCode::EmptyInput);
    }

    #[test]
    fn rejects_invalid_email() {
        assert_eq!(
            encode("A", "B", "", "", "", "not-an-email", "", "", "").unwrap_err().code(),
            ErrorCode::InvalidEmail
        );
    }
}
