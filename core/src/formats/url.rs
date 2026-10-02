//! Web address detection and encoding.
//!
//! VYNX QR is deliberately conservative: a value is only treated as a URL when
//! there is real evidence for it. When a scheme has to be added the caller is
//! told about it so the UI can show `https:// will be added` and let the user keep
//! the original text instead.

use crate::error::{err_with, AppResult, ErrorCode};

/// Outcome of inspecting a free-form value as a possible web address.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct UrlAnalysis {
    /// Exactly what would be encoded into the QR symbol.
    pub normalized: String,
    /// True when `https://` had to be inserted.
    pub added_scheme: bool,
}

const KNOWN_SCHEMES: [&str; 2] = ["http", "https"];

/// Decide whether `input` is a web address and how it would be normalised.
///
/// Returns `None` for anything that is not convincing enough, including values
/// that merely contain a dot (`v1.2`, `readme.md` is a borderline case that is
/// reported as a URL with an explicit `https://` notice).
pub fn analyze(input: &str) -> Option<UrlAnalysis> {
    let trimmed = input.trim();
    if trimmed.is_empty() || trimmed.len() > 2048 {
        return None;
    }
    // Raw whitespace is never valid inside a URL; a space usually means the
    // value is a sentence rather than an address.
    if trimmed.chars().any(char::is_whitespace) {
        return None;
    }
    if trimmed.chars().any(char::is_control) {
        return None;
    }

    // Scheme-relative addresses such as `//example.com/x`.
    if let Some(rest) = trimmed.strip_prefix("//") {
        if host_and_tail(rest).is_some() {
            return Some(UrlAnalysis {
                normalized: format!("https:{trimmed}"),
                added_scheme: true,
            });
        }
        return None;
    }

    if let Some((scheme, rest)) = split_scheme(trimmed) {
        let scheme = scheme.to_ascii_lowercase();
        if !KNOWN_SCHEMES.contains(&scheme.as_str()) {
            // `mailto:`, `tel:`, `WIFI:` and friends are left as plain text so the
            // payload is never rewritten behind the user's back.
            return None;
        }
        host_and_tail(rest)?;
        // Lower-case the scheme only; the rest of the address is user data and is
        // never rewritten.
        return Some(UrlAnalysis {
            normalized: format!("{scheme}://{}", authority_and_tail(trimmed)),
            added_scheme: false,
        });
    }

    // No scheme at all: only accept a well formed authority.
    let authority = split_authority_and_tail(trimmed).0;
    if valid_authority(authority) {
        return Some(UrlAnalysis { normalized: format!("https://{trimmed}"), added_scheme: true });
    }
    None
}

/// Validate a value that is supposed to already be a URL and return it unchanged.
pub fn encode(url: &str) -> AppResult<String> {
    let trimmed = url.trim();
    if trimmed.is_empty() {
        return Err(err_with(ErrorCode::InvalidUrl, "empty value"));
    }
    if trimmed.chars().any(char::is_whitespace) {
        return Err(err_with(ErrorCode::InvalidUrl, "contains whitespace"));
    }
    let Some((scheme, rest)) = split_scheme(trimmed) else {
        return Err(err_with(ErrorCode::InvalidUrl, "missing scheme"));
    };
    let scheme = scheme.to_ascii_lowercase();
    if !KNOWN_SCHEMES.contains(&scheme.as_str()) {
        return Err(err_with(ErrorCode::InvalidUrl, format!("unsupported scheme `{scheme}`")));
    }
    if host_and_tail(rest).is_none() {
        return Err(err_with(ErrorCode::InvalidUrl, "missing host"));
    }
    Ok(trimmed.to_string())
}

fn authority_and_tail(url: &str) -> String {
    let rest = url.split_once("://").map(|(_, rest)| rest).unwrap_or(url);
    rest.to_string()
}

/// Split `scheme:` from the rest, but only for hierarchical `scheme://` values.
/// A bare `host:8080/path` is a port, not a scheme, and opaque schemes such as
/// `mailto:` or `WIFI:` are never URL candidates, so both fall through.
fn split_scheme(value: &str) -> Option<(&str, &str)> {
    let index = value.find(':')?;
    let scheme = &value[..index];
    if scheme.is_empty() || !scheme.starts_with(|c: char| c.is_ascii_alphabetic()) {
        return None;
    }
    if !scheme
        .chars()
        .all(|c| c.is_ascii_alphanumeric() || c == '+' || c == '-' || c == '.')
    {
        return None;
    }
    let rest = &value[index + 1..];
    if !rest.starts_with("//") {
        return None;
    }
    Some((scheme, rest))
}

fn split_authority_and_tail(value: &str) -> (&str, &str) {
    match value.find(['/', '?', '#']) {
        Some(index) => (&value[..index], &value[index..]),
        None => (value, ""),
    }
}

/// `rest` is everything after the scheme, including the `//` when present.
fn host_and_tail(rest: &str) -> Option<&str> {
    let after_slashes = rest.strip_prefix("//").unwrap_or(rest);
    let authority = split_authority_and_tail(after_slashes).0;
    if authority.is_empty() {
        return None;
    }
    Some(after_slashes)
}

/// Validate `host[:port]` where `host` is a domain name, a single label such as
/// `localhost`, or an IPv4 literal.
fn valid_authority(authority: &str) -> bool {
    if authority.is_empty() || authority.len() > 253 {
        return false;
    }
    let (host, port) = match authority.rsplit_once(':') {
        Some((host, port)) if !port.is_empty() && port.chars().all(|c| c.is_ascii_digit()) => {
            (host, Some(port))
        }
        Some(_) => return false,
        None => (authority, None),
    };
    if let Some(port) = port {
        if port.len() > 5 || port.parse::<u32>().map(|p| p > 65535).unwrap_or(true) {
            return false;
        }
    }
    if host.is_empty() || host.starts_with('.') || host.ends_with('.') {
        return false;
    }
    // Single label hosts are only plausible for local development.
    if !host.contains('.') {
        return host.eq_ignore_ascii_case("localhost");
    }
    let labels: Vec<&str> = host.split('.').collect();
    if labels.iter().any(|label| !valid_label(label)) {
        return false;
    }
    if is_ipv4(&labels) {
        return true;
    }
    let tld = labels[labels.len() - 1];
    // Require an alphabetic top level domain. This keeps version-like strings
    // (`1.5`, `2.10.3`) out of the URL branch.
    (2..=24).contains(&tld.chars().count()) && tld.chars().all(|c| c.is_alphabetic())
}

fn is_ipv4(labels: &[&str]) -> bool {
    labels.len() == 4
        && labels.iter().all(|label| {
            !label.is_empty()
                && label.len() <= 3
                && label.chars().all(|c| c.is_ascii_digit())
                && label.parse::<u16>().map(|o| o <= 255).unwrap_or(false)
        })
}

fn valid_label(label: &str) -> bool {
    if label.is_empty() || label.len() > 63 {
        return false;
    }
    if label.starts_with('-') || label.ends_with('-') {
        return false;
    }
    label
        .chars()
        .all(|c| c.is_alphanumeric() || c == '-' || !c.is_ascii())
}

#[cfg(test)]
mod tests {
    use super::*;

    fn normalized(input: &str) -> Option<String> {
        analyze(input).map(|analysis| analysis.normalized)
    }

    #[test]
    fn keeps_explicit_http_and_https_untouched() {
        assert_eq!(normalized("https://example.com"), Some("https://example.com".into()));
        assert_eq!(normalized("http://example.com/a?b=c#d"), Some("http://example.com/a?b=c#d".into()));
        assert_eq!(normalized("HTTPS://Example.com"), Some("https://Example.com".into()));
    }

    #[test]
    fn adds_scheme_to_bare_domains() {
        assert_eq!(normalized("github.com/vynx"), Some("https://github.com/vynx".into()));
        assert_eq!(normalized("github.com"), Some("https://github.com".into()));
        assert_eq!(normalized("www.example.com"), Some("https://www.example.com".into()));
        assert_eq!(
            normalized("youtube.com/watch?v=dQw4w9WgXcQ"),
            Some("https://youtube.com/watch?v=dQw4w9WgXcQ".into())
        );
        assert_eq!(normalized("example.com:8080/x"), Some("https://example.com:8080/x".into()));
    }

    #[test]
    fn reports_whether_a_scheme_was_added() {
        assert!(analyze("github.com").expect("url").added_scheme);
        assert!(!analyze("https://github.com").expect("url").added_scheme);
    }

    #[test]
    fn handles_scheme_relative_addresses() {
        assert_eq!(normalized("//example.com/x"), Some("https://example.com/x".into()));
    }

    #[test]
    fn rejects_values_that_are_not_addresses() {
        for input in [
            "",
            "   ",
            "hello world",
            "1.5",
            "2.10.3",
            "readme.md extra",
            "what is this?",
            "-bad-.com",
            "example..com",
            "http://",
            "https:// space.com",
            "mailto:hi@example.com",
            "WIFI:T:WPA;S:x;;",
            "tel:+380991234567",
        ] {
            assert_eq!(normalized(input), None, "expected `{input}` not to be a URL");
        }
    }

    #[test]
    fn accepts_plain_language_versions_and_sentences_as_text() {
        // Sentences contain whitespace and therefore never reach the URL branch.
        assert_eq!(normalized("Привіт, Україно"), None);
    }

    #[test]
    fn accepts_ipv4_literals() {
        assert_eq!(normalized("192.168.0.1"), Some("https://192.168.0.1".into()));
        assert_eq!(normalized("127.0.0.1:8080"), Some("https://127.0.0.1:8080".into()));
        assert_eq!(normalized("999.1.1.1"), None);
    }

    #[test]
    fn encode_rejects_unsupported_schemes() {
        assert_eq!(encode("ftp://example.com").unwrap_err().code(), ErrorCode::InvalidUrl);
        assert_eq!(encode("example.com").unwrap_err().code(), ErrorCode::InvalidUrl);
        assert_eq!(encode("").unwrap_err().code(), ErrorCode::InvalidUrl);
    }

    #[test]
    fn encode_returns_normalised_value() {
        assert_eq!(encode("  https://example.com/x  ").expect("encode"), "https://example.com/x");
    }
}
