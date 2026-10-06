//! Deterministic content detection.
//!
//! No heuristics that need a model, no network, no heuristics that change
//! between runs: the same string always produces the same classification, which
//! is what makes the behaviour testable and predictable.

use crate::error::AppResult;
use crate::formats::{phone, url};
use crate::qr::payload::QrPayload;

/// Broad bucket the main input field resolved to.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ContentKind {
    Url,
    Email,
    Phone,
    Text,
}

impl ContentKind {
    pub const fn label(self) -> &'static str {
        match self {
            Self::Url => "URL",
            Self::Email => "Email",
            Self::Phone => "Phone",
            Self::Text => "Text",
        }
    }
}

/// Result of analysing the free-form input field.
#[derive(Debug, Clone, PartialEq)]
pub struct Analysis {
    /// Exactly what the user typed.
    pub input: String,
    /// Broad category, used for the semantic chip.
    pub kind: ContentKind,
    /// Payload that will be encoded.
    pub payload: QrPayload,
    /// The encoded QR string, handy for previews and tests.
    pub encoded: String,
    /// Present when VYNX QR would add `https://`, so the UI can say so.
    pub normalization: Option<String>,
    /// Non fatal notes, e.g. "URL detected".
    pub notice: Option<String>,
}

/// True for values that look like an email address.
pub fn looks_like_email(input: &str) -> bool {
    let value = input.trim();
    if value.is_empty() || value.len() > 254 || value.chars().any(char::is_whitespace) {
        return false;
    }
    if value.matches('@').count() != 1 {
        return false;
    }
    let Some((local, domain)) = value.split_once('@') else {
        return false;
    };
    if local.is_empty() || local.len() > 64 || local.starts_with('.') || local.ends_with('.') {
        return false;
    }
    if local.contains("..") {
        return false;
    }
    // Dot-atom only. This keeps `mailto:hi@example.com` out: the scheme would
    // otherwise be encoded a second time.
    if !local
        .chars()
        .all(|c| c.is_ascii_alphanumeric() || "!#$%&'*+/=?^_`{|}~.".contains(c))
    {
        return false;
    }
    if domain.is_empty() || domain.len() > 253 || domain.starts_with('.') || domain.ends_with('.') {
        return false;
    }
    if !domain.contains('.') {
        return false;
    }
    if domain.contains("..") {
        return false;
    }
    let labels: Vec<&str> = domain.split('.').collect();
    if labels.len() < 2 {
        return false;
    }
    labels.iter().all(|label| {
        !label.is_empty()
            && label.len() <= 63
            && !label.starts_with('-')
            && !label.ends_with('-')
            && label
                .chars()
                .all(|c| c.is_alphanumeric() || c == '-' || c == '_')
    }) && labels[labels.len() - 1].chars().all(|c| c.is_alphabetic())
}

/// Classify a free-form value.
///
/// Precedence is deliberate: URL wins over phone, phone wins over text, and an
/// email address is only ever reported as an email address — never as text and
/// never as a URL, even though `hello@example.com` contains both an `@` and
/// domain-shaped dots.
pub fn analyze_input(input: &str) -> Option<Analysis> {
    let trimmed = input.trim();
    if trimmed.is_empty() {
        return None;
    }

    if let Some(url_analysis) = url::analyze(trimmed) {
        let payload = QrPayload::Url {
            url: url_analysis.normalized.clone(),
        };
        let encoded = payload.encode().ok()?;
        let normalization = if url_analysis.added_scheme {
            Some(format!(
                "{} will be added",
                scheme_prefix(&url_analysis.normalized)
            ))
        } else {
            None
        };
        return Some(Analysis {
            input: input.to_string(),
            kind: ContentKind::Url,
            payload,
            encoded,
            normalization,
            notice: None,
        });
    }

    if let Some(address) = trimmed.strip_prefix("mailto:") {
        let address = address.split(['?', '#']).next().unwrap_or(address);
        let address = address.trim();
        if looks_like_email(address) {
            let payload = QrPayload::Email {
                to: address.to_string(),
                subject: String::new(),
                body: String::new(),
            };
            let encoded = payload.encode().ok()?;
            return Some(Analysis {
                input: input.to_string(),
                kind: ContentKind::Email,
                payload,
                encoded,
                normalization: None,
                notice: None,
            });
        }
        return None;
    }

    if looks_like_email(trimmed) {
        let payload = QrPayload::Email {
            to: trimmed.to_string(),
            subject: String::new(),
            body: String::new(),
        };
        let encoded = payload.encode().ok()?;
        return Some(Analysis {
            input: input.to_string(),
            kind: ContentKind::Email,
            payload,
            encoded,
            normalization: None,
            notice: None,
        });
    }

    if phone::looks_like_phone(trimmed) {
        let payload = QrPayload::Phone {
            number: phone::normalize(trimmed),
        };
        let encoded = payload.encode().ok()?;
        return Some(Analysis {
            input: input.to_string(),
            kind: ContentKind::Phone,
            payload,
            encoded,
            normalization: None,
            notice: None,
        });
    }

    let payload = QrPayload::Text {
        text: trimmed.to_string(),
    };
    let encoded = payload.encode().ok()?;
    Some(Analysis {
        input: input.to_string(),
        kind: ContentKind::Text,
        payload,
        encoded,
        normalization: None,
        notice: None,
    })
}

/// Variant of [`analyze_input`] that never fails: invalid values come back as an
/// error the UI can display inline.
pub fn analyze_input_checked(input: &str) -> AppResult<Option<Analysis>> {
    Ok(analyze_input(input))
}

fn scheme_prefix(url: &str) -> &'static str {
    if url.starts_with("https://") {
        "https://"
    } else {
        "http://"
    }
}

/// Build the "keep what I typed" alternative for a detected URL.
pub fn original_text_payload(input: &str) -> QrPayload {
    QrPayload::Text {
        text: input.to_string(),
    }
}

impl Analysis {
    /// True when the encoded form differs from what was typed, so the UI has
    /// something worth telling the user about.
    ///
    /// This is deliberately asked of the domain model rather than inferred by
    /// comparing strings in the presentation layer: only the detector knows
    /// whether a change was a normalisation it chose to make, or merely a
    /// re-render of the same characters.
    pub fn changes_the_input(&self) -> bool {
        self.encoded != self.input.trim()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn kind_of(input: &str) -> Option<ContentKind> {
        analyze_input(input).map(|analysis| analysis.kind)
    }

    #[test]
    fn detects_urls() {
        assert_eq!(kind_of("https://github.com/"), Some(ContentKind::Url));
        assert_eq!(kind_of("http://example.com"), Some(ContentKind::Url));
        assert_eq!(kind_of("github.com"), Some(ContentKind::Url));
        assert_eq!(
            kind_of("youtube.com/watch?v=dQw4w9WgXcQ"),
            Some(ContentKind::Url)
        );
    }

    #[test]
    fn announces_url_normalisation() {
        let analysis = analyze_input("github.com/vynx").expect("analysis");
        assert_eq!(
            analysis.normalization.as_deref(),
            Some("https:// will be added")
        );
        assert_eq!(analysis.encoded, "https://github.com/vynx");
    }

    #[test]
    fn does_not_announce_normalisation_for_real_urls() {
        let analysis = analyze_input("https://github.com/vynx").expect("analysis");
        assert_eq!(analysis.normalization, None);
        assert_eq!(analysis.encoded, "https://github.com/vynx");
    }

    #[test]
    fn detects_email_before_text() {
        let analysis = analyze_input("hello@example.com").expect("analysis");
        assert_eq!(analysis.kind, ContentKind::Email);
        assert_eq!(analysis.encoded, "mailto:hello@example.com");
    }

    #[test]
    fn detects_phone_without_being_aggressive() {
        assert_eq!(kind_of("+380991234567"), Some(ContentKind::Phone));
        assert_eq!(kind_of("+1 (555) 010-9999"), Some(ContentKind::Phone));
        assert_eq!(kind_of("380991234567"), Some(ContentKind::Text));
        assert_eq!(kind_of("1234567"), Some(ContentKind::Text));
        assert_eq!(kind_of("Version 1.2.3"), Some(ContentKind::Text));
    }

    #[test]
    fn falls_back_to_text() {
        let analysis = analyze_input("Привіт, Україно 🇺🇦").expect("analysis");
        assert_eq!(analysis.kind, ContentKind::Text);
        assert_eq!(analysis.encoded, "Привіт, Україно 🇺🇦");
    }

    #[test]
    fn keeps_leading_and_trailing_whitespace_out_of_the_payload() {
        let analysis = analyze_input("  https://example.com  ").expect("analysis");
        assert_eq!(analysis.encoded, "https://example.com");
        assert_eq!(analysis.input, "  https://example.com  ");
    }

    #[test]
    fn empty_input_produces_nothing() {
        assert_eq!(kind_of(""), None);
        assert_eq!(kind_of("    "), None);
    }

    #[test]
    fn other_schemes_stay_text() {
        assert_eq!(kind_of("WIFI:T:WPA;S:x;;"), Some(ContentKind::Text));
        assert_eq!(kind_of("geo:1,2"), Some(ContentKind::Text));
    }

    #[test]
    fn a_tel_uri_keeps_its_scheme_exactly_once() {
        let analysis = analyze_input("tel:+380991234567").expect("analysis");
        assert_eq!(analysis.kind, ContentKind::Phone);
        assert_eq!(analysis.encoded, "tel:+380991234567");
    }

    #[test]
    fn a_mailto_uri_keeps_its_scheme_exactly_once() {
        let analysis = analyze_input("mailto:hi@example.com").expect("analysis");
        assert_eq!(analysis.kind, ContentKind::Email);
        assert_eq!(analysis.encoded, "mailto:hi@example.com");
    }

    #[test]
    fn email_predicate_is_strict() {
        assert!(looks_like_email("hello@example.com"));
        assert!(looks_like_email("a.b+c@sub.example.co.uk"));
        assert!(!looks_like_email("hello@example"));
        assert!(!looks_like_email("@example.com"));
        assert!(!looks_like_email("hello world@example.com"));
        assert!(!looks_like_email("hello@.com"));
        assert!(!looks_like_email("hello..world@example.com"));
        assert!(!looks_like_email("hello@example.123"));
        assert!(!looks_like_email("mailto:hi@example.com"));
        assert!(!looks_like_email("a b@example.com"));
    }
}
