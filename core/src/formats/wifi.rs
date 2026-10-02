//! Wi-Fi (`WIFI:`) payloads.

use crate::error::{err_with, AppResult, ErrorCode};
use crate::formats::escape_wifi_value;
use crate::qr::payload::WifiSecurity;

/// Build a standard `WIFI:` payload.
///
/// The payload ends with a trailing `;` because that is what every major phone
/// camera expects, and all values are backslash escaped.
pub fn encode(
    ssid: &str,
    password: &str,
    security: WifiSecurity,
    hidden: bool,
) -> AppResult<String> {
    let ssid = ssid.trim();
    if ssid.is_empty() {
        return Err(err_with(ErrorCode::InvalidWifi, "ssid is blank"));
    }
    if ssid.chars().count() > 32 {
        return Err(err_with(ErrorCode::InvalidWifi, "ssid longer than 32 characters"));
    }
    if ssid.contains('\n') || ssid.contains('\r') {
        return Err(err_with(ErrorCode::InvalidWifi, "ssid contains a line break"));
    }

    let password = password.trim();
    if security != WifiSecurity::None && password.is_empty() {
        return Err(err_with(ErrorCode::InvalidWifi, "password required for this security type"));
    }
    if password.chars().count() > 63 {
        return Err(err_with(ErrorCode::InvalidWifi, "password longer than 63 characters"));
    }
    if password.contains('\n') || password.contains('\r') {
        return Err(err_with(ErrorCode::InvalidWifi, "password contains a line break"));
    }

    let mut payload = String::with_capacity(64 + ssid.len() + password.len());
    payload.push_str("WIFI:");
    payload.push_str(&format!("T:{};", security.token()));
    payload.push_str(&format!("S:{};", escape_wifi_value(ssid)));
    if security != WifiSecurity::None {
        payload.push_str(&format!("P:{};", escape_wifi_value(password)));
    }
    if hidden {
        payload.push_str("H:true;");
    }
    payload.push(';');
    Ok(payload)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn encodes_a_typical_home_network() {
        let payload = encode("VYNX Home", "secret123", WifiSecurity::Wpa, false).expect("encode");
        assert_eq!(payload, "WIFI:T:WPA;S:VYNX Home;P:secret123;;");
    }

    #[test]
    fn escapes_delimiters_in_ssid_and_password() {
        let payload =
            encode("My;Password:123", "p,a\\ss", WifiSecurity::Wpa, false).expect("encode");
        assert_eq!(payload, r"WIFI:T:WPA;S:My\;Password\:123;P:p\,a\\ss;;");
    }

    #[test]
    fn marks_hidden_networks() {
        let payload = encode("Hidden", "pw", WifiSecurity::Wpa, true).expect("encode");
        assert_eq!(payload, "WIFI:T:WPA;S:Hidden;P:pw;H:true;;");
    }

    #[test]
    fn omits_password_for_open_networks() {
        let payload = encode("Cafe", "", WifiSecurity::None, false).expect("encode");
        assert_eq!(payload, "WIFI:T:nopass;S:Cafe;;");
    }

    #[test]
    fn supports_wep() {
        let payload = encode("Old", "wepkey", WifiSecurity::Wep, false).expect("encode");
        assert_eq!(payload, "WIFI:T:WEP;S:Old;P:wepkey;;");
    }

    #[test]
    fn rejects_invalid_input() {
        assert_eq!(encode("  ", "pw", WifiSecurity::Wpa, false).unwrap_err().code(), ErrorCode::InvalidWifi);
        assert_eq!(encode("SSID", "", WifiSecurity::Wpa, false).unwrap_err().code(), ErrorCode::InvalidWifi);
        assert!(encode("SSID", "pw", WifiSecurity::Wep, false).is_ok());
        assert_eq!(
            encode(&"a".repeat(33), "pw", WifiSecurity::Wpa, false).unwrap_err().code(),
            ErrorCode::InvalidWifi
        );
    }

    #[test]
    fn keeps_unicode_network_names() {
        let payload = encode("Мій Дім", "пароль", WifiSecurity::Wpa, false).expect("encode");
        assert_eq!(payload, "WIFI:T:WPA;S:Мій Дім;P:пароль;;");
    }
}
