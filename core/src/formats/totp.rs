//! Two-Factor Authentication (TOTP / `otpauth://`) payloads.

use crate::error::{err_with, AppResult, ErrorCode};
use crate::formats::percent_encode;

/// Build an `otpauth://totp/` URI for authenticator apps.
pub fn encode(
    secret: &str,
    label: &str,
    issuer: &str,
    digits: u32,
    period: u32,
) -> AppResult<String> {
    let secret = secret.trim().replace(' ', "").to_uppercase();
    if secret.is_empty() {
        return Err(err_with(
            ErrorCode::EmptyInput,
            "TOTP secret key cannot be empty",
        ));
    }

    let label = label.trim();
    let issuer = issuer.trim();

    let full_label = if !issuer.is_empty() && !label.is_empty() {
        format!("{}:{}", percent_encode(issuer), percent_encode(label))
    } else if !label.is_empty() {
        percent_encode(label)
    } else if !issuer.is_empty() {
        percent_encode(issuer)
    } else {
        "Account".to_string()
    };

    let mut uri = format!("otpauth://totp/{full_label}?secret={secret}");

    if !issuer.is_empty() {
        uri.push_str(&format!("&issuer={}", percent_encode(issuer)));
    }
    if digits != 6 && digits > 0 {
        uri.push_str(&format!("&digits={digits}"));
    }
    if period != 30 && period > 0 {
        uri.push_str(&format!("&period={period}"));
    }

    Ok(uri)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn totp_encoding() {
        let uri = encode("JBSWY3DPEHPK3PXP", "user@example.com", "GitHub", 6, 30).unwrap();
        assert!(uri.starts_with("otpauth://totp/GitHub:user%40example.com?secret=JBSWY3DPEHPK3PXP"));
        assert!(uri.contains("&issuer=GitHub"));
    }
}
