//! Payload model.
//!
//! The very same enum is used for detection results, style requests and export
//! requests, so the UI never has to hand raw JSON blobs across the bridge.

use serde::{Deserialize, Serialize};

use crate::error::{err_with, AppResult, ErrorCode};

/// Wi-Fi authentication variants that have a defined `WIFI:` payload encoding.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Default)]
pub enum WifiSecurity {
    #[default]
    Wpa,
    Wep,
    None,
}

impl WifiSecurity {
    pub const ALL: [Self; 3] = [Self::Wpa, Self::Wep, Self::None];

    /// Token used in the `WIFI:` payload.
    pub const fn token(self) -> &'static str {
        match self {
            Self::Wpa => "WPA",
            Self::Wep => "WEP",
            Self::None => "nopass",
        }
    }

    pub const fn label(self) -> &'static str {
        match self {
            Self::Wpa => "WPA / WPA2 / WPA3",
            Self::Wep => "WEP",
            Self::None => "None (open)",
        }
    }
}

/// Error correction level.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize, Default)]
pub enum EcLevel {
    L,
    #[default]
    M,
    Q,
    H,
}

impl EcLevel {
    pub const ALL: [Self; 4] = [Self::L, Self::M, Self::Q, Self::H];

    pub const fn label(self) -> &'static str {
        match self {
            Self::L => "Low",
            Self::M => "Medium",
            Self::Q => "Quartile",
            Self::H => "High",
        }
    }

    /// Recovery capacity as a percentage of codewords.
    pub const fn recovery_percent(self) -> f32 {
        match self {
            Self::L => 7.0,
            Self::M => 15.0,
            Self::Q => 25.0,
            Self::H => 30.0,
        }
    }
}

impl From<EcLevel> for qrcode::EcLevel {
    fn from(value: EcLevel) -> Self {
        match value {
            EcLevel::L => Self::L,
            EcLevel::M => Self::M,
            EcLevel::Q => Self::Q,
            EcLevel::H => Self::H,
        }
    }
}

/// Module rendering style.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize, Default)]
pub enum ModuleStyle {
    #[default]
    Square,
    Rounded,
}

/// Everything VYNX QR knows how to put inside a QR code.
///
/// This is the type the bridge shares with C++, so a caller never passes an
/// untyped blob: the shape is checked by both compilers.
#[derive(Debug, Clone, PartialEq)]
pub enum QrPayload {
    Text {
        text: String,
    },
    Url {
        url: String,
    },
    Wifi {
        ssid: String,
        password: String,
        security: WifiSecurity,
        hidden: bool,
    },
    VCard {
        first_name: String,
        last_name: String,
        organization: String,
        job_title: String,
        phone: String,
        email: String,
        website: String,
        address: String,
        note: String,
    },
    Email {
        to: String,
        subject: String,
        body: String,
    },
    Phone {
        number: String,
    },
    Sms {
        number: String,
        message: String,
    },
    Geo {
        latitude: f64,
        longitude: f64,
        label: String,
    },
}

impl QrPayload {
    /// Stable identifier used for telemetry-free status text and file naming.
    pub const fn kind(&self) -> &'static str {
        match self {
            Self::Text { .. } => "text",
            Self::Url { .. } => "url",
            Self::Wifi { .. } => "wifi",
            Self::VCard { .. } => "vcard",
            Self::Email { .. } => "email",
            Self::Phone { .. } => "phone",
            Self::Sms { .. } => "sms",
            Self::Geo { .. } => "geo",
        }
    }

    /// Encode the payload into the string that will be stored in the QR symbol.
    pub fn encode(&self) -> AppResult<String> {
        match self {
            Self::Text { text } => crate::formats::text::encode(text),
            Self::Url { url } => crate::formats::url::encode(url),
            Self::Wifi {
                ssid,
                password,
                security,
                hidden,
            } => crate::formats::wifi::encode(ssid, password, *security, *hidden),
            Self::VCard {
                first_name,
                last_name,
                organization,
                job_title,
                phone,
                email,
                website,
                address,
                note,
            } => crate::formats::vcard::encode(
                first_name,
                last_name,
                organization,
                job_title,
                phone,
                email,
                website,
                address,
                note,
            ),
            Self::Email { to, subject, body } => crate::formats::email::encode(to, subject, body),
            Self::Phone { number } => crate::formats::phone::encode(number),
            Self::Sms { number, message } => crate::formats::sms::encode(number, message),
            Self::Geo {
                latitude,
                longitude,
                label,
            } => crate::formats::geo::encode(*latitude, *longitude, label),
        }
    }

    /// A short human readable label, used for the status line and file names.
    pub fn display_label(&self) -> String {
        match self {
            Self::Text { text } => {
                let flat = text.split_whitespace().collect::<Vec<_>>().join(" ");
                flat.chars().take(48).collect()
            }
            Self::Url { url } => url.clone(),
            Self::Wifi { ssid, .. } => ssid.clone(),
            Self::VCard {
                first_name,
                last_name,
                ..
            } => {
                let name = format!("{first_name} {last_name}").trim().to_string();
                if name.is_empty() {
                    "Contact".to_string()
                } else {
                    name
                }
            }
            Self::Email { to, .. } => to.clone(),
            Self::Phone { number } => number.clone(),
            Self::Sms { number, .. } => number.clone(),
            Self::Geo { label, .. } => {
                if label.trim().is_empty() {
                    "Location".to_string()
                } else {
                    label.clone()
                }
            }
        }
    }
}

/// Convenience constructor used by the vCard form so the front end does not have
/// to spell out nine empty strings.
impl QrPayload {
    pub fn vcard_empty() -> Self {
        Self::VCard {
            first_name: String::new(),
            last_name: String::new(),
            organization: String::new(),
            job_title: String::new(),
            phone: String::new(),
            email: String::new(),
            website: String::new(),
            address: String::new(),
            note: String::new(),
        }
    }
}

/// Reject payloads that QR codes cannot represent at all.
pub fn ensure_not_empty(payload: &QrPayload) -> AppResult<()> {
    match payload {
        QrPayload::Text { text } if text.trim().is_empty() => {
            Err(crate::error::err(ErrorCode::EmptyInput))
        }
        QrPayload::Url { url } if url.trim().is_empty() => {
            Err(err_with(ErrorCode::InvalidUrl, "empty url"))
        }
        _ => Ok(()),
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn ec_level_round_trips_through_qrcode_crate() {
        for level in EcLevel::ALL {
            let mapped: qrcode::EcLevel = level.into();
            match (level, mapped) {
                (EcLevel::L, qrcode::EcLevel::L)
                | (EcLevel::M, qrcode::EcLevel::M)
                | (EcLevel::Q, qrcode::EcLevel::Q)
                | (EcLevel::H, qrcode::EcLevel::H) => {}
                other => panic!("unexpected mapping: {other:?}"),
            }
        }
    }

    #[test]
    fn phone_payload_reports_its_kind() {
        let payload = QrPayload::Phone {
            number: "+380991234567".into(),
        };
        assert_eq!(payload.kind(), "phone");
        assert_eq!(payload.encode().expect("encode"), "tel:+380991234567");
    }

    #[test]
    fn vcard_payload_summarises_the_contact() {
        let payload = QrPayload::VCard {
            first_name: "Олена".into(),
            last_name: "К".into(),
            organization: String::new(),
            job_title: String::new(),
            phone: "+380".into(),
            email: String::new(),
            website: String::new(),
            address: String::new(),
            note: String::new(),
        };
        assert_eq!(payload.kind(), "vcard");
        assert_eq!(payload.display_label(), "Олена К");
    }

    /// The bridge moves these strings by value, so anything the UI can type has to
    /// survive the round trip through the engine untouched.
    #[test]
    fn unicode_survives_untouched() {
        for text in [
            "Привіт, Україно 🇺🇦",
            "こんにちは",
            "مرحبا",
            "🙂",
            "a\u{200B}b",
        ] {
            let payload = QrPayload::Text {
                text: text.to_string(),
            };
            let encoded = payload.encode().expect("encode");
            assert_eq!(encoded, text);
        }
    }
}
