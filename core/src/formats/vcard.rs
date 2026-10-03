//! vCard 3.0 contact payloads.
//!
//! vCard 3.0 is used because it is the version understood by the largest number
//! of phone camera scanners and by Outlook, Google Contacts and the Apple
//! address book alike. Values are escaped per RFC 2426 §5, lines are folded at 75
//! octets per RFC 2425 §5.8.1.1, and the record is joined with CRLF.
//!
//! The address is emitted as a properly structured `ADR` with the free text in the
//! street component rather than being spread across the seven slots. A scanner
//! reads `ADR;TYPE=HOME:Kyiv, Ukraine` as po-box `Kyiv` plus extended address
//! `Ukraine`, which is not what anyone typing a postal address meant.

use crate::error::{err_with, AppResult, ErrorCode};

/// Maximum length of a content line before it is folded, in octets.
const FOLD_LIMIT: usize = 75;

/// Escape a vCard value: backslash, comma, semicolon and newlines.
pub fn escape_value(value: &str) -> String {
    let mut out = String::with_capacity(value.len() + 8);
    for ch in value.chars() {
        match ch {
            '\\' => out.push_str("\\\\"),
            ';' => out.push_str("\\;"),
            ',' => out.push_str("\\,"),
            '\n' => out.push_str("\\n"),
            // A bare carriage return would end the line early; the newline above
            // already represents the break.
            '\r' => {}
            other => out.push(other),
        }
    }
    out
}

/// Fold a content line to {@link FOLD_LIMIT} octets.
///
/// Folding inserts CRLF followed by a single space, which a reader strips. The
/// break is placed on a character boundary so a multi-byte character is never
/// cut in half, which would make the record undecodable.
fn fold_line(line: &str) -> String {
    if line.len() <= FOLD_LIMIT {
        return line.to_string();
    }

    let mut out = String::with_capacity(line.len() + 16);
    let mut used = 0usize;
    for ch in line.chars() {
        let width = ch.len_utf8();
        // A continuation line starts with a space, so it carries one octet less.
        let limit = if out.is_empty() {
            FOLD_LIMIT
        } else {
            FOLD_LIMIT - 1
        };
        if used + width > limit {
            out.push_str("\r\n ");
            used = 1;
        }
        out.push(ch);
        used += width;
    }
    out
}

/// Build a structured `ADR` line with the free text in the street component.
///
/// The seven components are po-box, extended address, street, locality, region,
/// postal code and country. Only the street is filled, because that is the only
/// one the user actually supplied.
fn address_line(address: &str) -> String {
    format!("ADR;TYPE=HOME:;;{};;;", escape_value(address.trim()))
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
    if [
        first_name,
        last_name,
        organization,
        job_title,
        phone,
        email,
        website,
        address,
        note,
    ]
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
        lines.push(
            "N:".to_string() + &escape_value(last_name) + ";" + &escape_value(first_name) + ";;;",
        );
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
    if !address.trim().is_empty() {
        lines.push(address_line(address));
    }
    push_if_present(&mut lines, "NOTE", note);
    lines.push("END:VCARD".into());

    let folded: Vec<String> = lines.iter().map(|line| fold_line(line)).collect();
    Ok(folded.join("\r\n"))
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
        // Structured: the text lands in the street slot, not smeared over the
        // seven components.
        assert!(payload.contains(r"ADR;TYPE=HOME:;;Kyiv\, Ukraine;;;"));
    }

    #[test]
    fn address_is_a_structured_adr() {
        let payload =
            encode("A", "B", "", "", "", "", "", "1 Main St, Kyiv, Ukraine", "").expect("encode");
        let line = payload
            .lines()
            .find(|l| l.starts_with("ADR"))
            .expect("ADR line");
        // Seven components: two leading empties, the street, four trailing empties.
        assert_eq!(line, r"ADR;TYPE=HOME:;;1 Main St\, Kyiv\, Ukraine;;;");
    }

    #[test]
    fn long_lines_are_folded_at_seventy_five_octets() {
        let note = "x".repeat(200);
        let payload = encode("A", "B", "", "", "", "", "", "", &note).expect("encode");
        let lines: Vec<&str> = payload.split("\r\n").collect();
        for line in &lines {
            assert!(
                line.len() <= FOLD_LIMIT,
                "line too long: {} octets",
                line.len()
            );
        }
        // Continuation lines start with the single space that the folding inserts.
        let continuations = lines.iter().filter(|line| line.starts_with(' ')).count();
        assert!(
            continuations >= 2,
            "a 200 octet note must be folded at least twice"
        );
        // Folding is transport, not content: unfolding restores the original.
        let unfolded = payload.replace("\r\n ", "");
        assert!(unfolded.contains(&format!("NOTE:{note}")));
    }

    #[test]
    fn folding_never_splits_a_multi_byte_character() {
        let note = "Я".repeat(80);
        let payload = encode("A", "B", "", "", "", "", "", "", &note).expect("encode");
        assert!(payload.contains("NOTE:"));
        // Every line is valid UTF-8 by construction; the content must survive.
        let unfolded = payload.replace("\r\n ", "");
        assert!(unfolded.contains(&note));
        for line in payload.split("\r\n") {
            assert!(line.len() <= FOLD_LIMIT);
        }
    }

    #[test]
    fn short_lines_are_left_alone() {
        let payload = full().expect("encode");
        assert!(
            !payload.contains("\r\n "),
            "nothing here is long enough to fold"
        );
    }

    #[test]
    fn escapes_delimiters_and_newlines() {
        let payload = encode(
            "A",
            "B",
            "",
            "",
            "",
            "",
            "",
            "",
            "line1\nline2; semi, comma",
        )
        .expect("encode");
        assert!(payload.contains("NOTE:line1\\nline2\\; semi\\, comma"));
    }

    #[test]
    fn keeps_emoji_and_non_latin_scripts() {
        let payload =
            encode("Taro", "山田", "", "", "", "", "", "", "こんにちは 🇯🇵").expect("encode");
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
        assert!(!payload.contains("ADR"));
    }

    #[test]
    fn rejects_empty_contact() {
        assert_eq!(
            encode("", "", "", "", "", "", "", "", "")
                .unwrap_err()
                .code(),
            ErrorCode::EmptyInput
        );
    }

    #[test]
    fn rejects_invalid_email() {
        assert_eq!(
            encode("A", "B", "", "", "", "not-an-email", "", "", "")
                .unwrap_err()
                .code(),
            ErrorCode::InvalidEmail
        );
    }
}
