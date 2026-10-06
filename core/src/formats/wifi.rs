//! Wi-Fi (`WIFI:`) payloads.

use crate::error::{err_with, AppResult, ErrorCode};
use crate::formats::escape_wifi_value;
use crate::qr::payload::WifiSecurity;

/// Build a standard `WIFI:` payload.
///
/// The payload ends with a trailing `;` because that is what every major phone
/// camera expects, and all values are backslash escaped.
///
/// Blank is checked with `trim`, but nothing is trimmed out of the value that
/// goes into the code. `" HomeWiFi "` and `"HomeWiFi"` are different networks,
/// and a password of `" password "` is not `"password"`. A leading or trailing
/// space in an SSID is legal and some access points broadcast one, so silently
/// removing it would produce a code that connects to the wrong thing.
pub fn encode(
    ssid: &str,
    password: &str,
    security: WifiSecurity,
    hidden: bool,
) -> AppResult<String> {
    if ssid.trim().is_empty() {
        return Err(err_with(ErrorCode::InvalidWifi, "ssid is blank"));
    }
    if ssid.len() > 32 {
        return Err(err_with(
            ErrorCode::InvalidWifi,
            "ssid longer than 32 UTF-8 bytes",
        ));
    }
    if ssid.contains('\n') || ssid.contains('\r') {
        return Err(err_with(
            ErrorCode::InvalidWifi,
            "ssid contains a line break",
        ));
    }

    if security != WifiSecurity::None && password.trim().is_empty() {
        return Err(err_with(
            ErrorCode::InvalidWifi,
            "password required for this security type",
        ));
    }
    let printable = password.bytes().all(|byte| (0x20..=0x7e).contains(&byte));
    let hex = password.bytes().all(|byte| byte.is_ascii_hexdigit());
    if security == WifiSecurity::Wpa
        && !((printable && (8..=63).contains(&password.len())) || (hex && password.len() == 64))
    {
        return Err(err_with(
            ErrorCode::InvalidWifi,
            "WPA needs 8–63 printable ASCII characters or 64 hexadecimal digits",
        ));
    }
    if security != WifiSecurity::None && (password.contains('\n') || password.contains('\r')) {
        return Err(err_with(
            ErrorCode::InvalidWifi,
            "password contains a line break",
        ));
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
            encode("My;Password:123", "p,a\\ss123", WifiSecurity::Wpa, false).expect("encode");
        assert_eq!(payload, r"WIFI:T:WPA;S:My\;Password\:123;P:p\,a\\ss123;;");
    }

    #[test]
    fn marks_hidden_networks() {
        let payload = encode("Hidden", "password", WifiSecurity::Wpa, true).expect("encode");
        assert_eq!(payload, "WIFI:T:WPA;S:Hidden;P:password;H:true;;");
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
        assert_eq!(
            encode("  ", "password", WifiSecurity::Wpa, false)
                .unwrap_err()
                .code(),
            ErrorCode::InvalidWifi
        );
        assert_eq!(
            encode("SSID", "", WifiSecurity::Wpa, false)
                .unwrap_err()
                .code(),
            ErrorCode::InvalidWifi
        );
        assert!(encode("SSID", "password", WifiSecurity::Wep, false).is_ok());
        assert_eq!(
            encode(&"a".repeat(33), "password", WifiSecurity::Wpa, false)
                .unwrap_err()
                .code(),
            ErrorCode::InvalidWifi
        );
    }

    #[test]
    fn keeps_unicode_network_names() {
        let payload = encode("Мій Дім", "password", WifiSecurity::Wpa, false).expect("encode");
        assert_eq!(payload, "WIFI:T:WPA;S:Мій Дім;P:password;;");
    }

    /// A leading or trailing space is part of the network name. Trimming it would
    /// hand the user a code for a different access point.
    #[test]
    fn preserves_surrounding_whitespace_in_the_ssid() {
        let payload = encode(" VYNX Home ", "password", WifiSecurity::Wpa, false).expect("encode");
        assert_eq!(payload, "WIFI:T:WPA;S: VYNX Home ;P:password;;");
    }

    #[test]
    fn preserves_surrounding_whitespace_in_the_password() {
        let payload =
            encode("Home", " password with spaces ", WifiSecurity::Wpa, false).expect("encode");
        assert_eq!(payload, "WIFI:T:WPA;S:Home;P: password with spaces ;;");
    }

    #[test]
    fn distinguishes_two_names_that_differ_only_in_padding() {
        let padded = encode("Home", "password", WifiSecurity::Wpa, false).expect("encode");
        let spaced = encode(" Home ", "password", WifiSecurity::Wpa, false).expect("encode");
        assert_ne!(padded, spaced);
    }

    #[test]
    fn still_rejects_a_blank_ssid_or_password() {
        assert_eq!(
            encode("   ", "password", WifiSecurity::Wpa, false)
                .unwrap_err()
                .code(),
            ErrorCode::InvalidWifi
        );
        assert_eq!(
            encode("\t\n", "password", WifiSecurity::Wpa, false)
                .unwrap_err()
                .code(),
            ErrorCode::InvalidWifi
        );
        assert_eq!(
            encode("SSID", "   ", WifiSecurity::Wpa, false)
                .unwrap_err()
                .code(),
            ErrorCode::InvalidWifi
        );
    }

    /// An open network has no password, so an empty one is expected rather than an
    /// error, padded or not.
    #[test]
    fn an_open_network_accepts_any_password_field() {
        let payload = encode("Cafe", "unused", WifiSecurity::None, false).expect("encode");
        assert_eq!(payload, "WIFI:T:nopass;S:Cafe;;");
    }

    #[test]
    fn enforces_utf8_byte_limit_for_ssid() {
        assert!(encode(&"é".repeat(16), "password", WifiSecurity::Wpa, false).is_ok());
        assert!(encode(&"é".repeat(17), "password", WifiSecurity::Wpa, false).is_err());
    }

    #[test]
    fn validates_wpa_passphrases_and_raw_keys() {
        for password in ["short", "пароль123", "password\n", "nothex"] {
            assert!(encode("Home", password, WifiSecurity::Wpa, false).is_err());
        }
        assert!(encode("Home", &"a".repeat(63), WifiSecurity::Wpa, false).is_ok());
        assert!(encode("Home", &"a".repeat(64), WifiSecurity::Wpa, false).is_ok());
        assert!(encode("Home", &"g".repeat(64), WifiSecurity::Wpa, false).is_err());
        assert!(encode("Home", &"a".repeat(65), WifiSecurity::Wpa, false).is_err());
        assert!(encode("Home", &"ignored\n".repeat(20), WifiSecurity::None, false).is_ok());
    }
}
