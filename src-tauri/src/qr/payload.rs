//! Payload model shared by the Rust core and the React front end.
//!
//! The very same enum is used for detection results, style requests and export
//! requests, so the UI never has to hand raw JSON blobs across the IPC bridge.

use serde::{Deserialize, Serialize};

use crate::error::{err_with, AppResult, ErrorCode};

/// Wi-Fi authentication variants that have a defined `WIFI:` payload encoding.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize, Default)]
#[serde(rename_all = "camelCase")]
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

/// Error correction level. Serialised in camelCase so the front end can use the
/// payload directly as an enum member.
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize, Default)]
#[serde(rename_all = "camelCase")]
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
#[serde(rename_all = "camelCase")]
pub enum ModuleStyle {
    #[default]
    Square,
    Rounded,
}

/// Everything VYNX QR knows how to put inside a QR code.
///
/// The enum is tagged by `type` and uses camelCase field names, which is the
/// exact shape `src/types/qr.ts` declares on the TypeScript side.
///
/// Every field except the identifying ones is `#[serde(default)]`, so a
/// partially filled form reaches the core without the UI padding every field.
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
#[serde(tag = "type", rename_all = "camelCase", rename_all_fields = "camelCase")]
pub enum QrPayload {
    Text {
        text: String,
    },
    Url {
        #[serde(default)]
        url: String,
    },
    Wifi {
        #[serde(default)]
        ssid: String,
        #[serde(default)]
        password: String,
        #[serde(default)]
        security: WifiSecurity,
        #[serde(default)]
        hidden: bool,
    },
    VCard {
        #[serde(default)]
        first_name: String,
        #[serde(default)]
        last_name: String,
        #[serde(default)]
        organization: String,
        #[serde(default)]
        job_title: String,
        #[serde(default)]
        phone: String,
        #[serde(default)]
        email: String,
        #[serde(default)]
        website: String,
        #[serde(default)]
        address: String,
        #[serde(default)]
        note: String,
    },
    Email {
        #[serde(default)]
        to: String,
        #[serde(default)]
        subject: String,
        #[serde(default)]
        body: String,
    },
    Phone {
        number: String,
    },
    Sms {
        #[serde(default)]
        number: String,
        #[serde(default)]
        message: String,
    },
    Geo {
        #[serde(default)]
        latitude: f64,
        #[serde(default)]
        longitude: f64,
        #[serde(default)]
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
            Self::Wifi { ssid, password, security, hidden } => {
                crate::formats::wifi::encode(ssid, password, *security, *hidden)
            }
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
            Self::Geo { latitude, longitude, label } => {
                crate::formats::geo::encode(*latitude, *longitude, label)
            }
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
            Self::VCard { first_name, last_name, .. } => {
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
    fn payload_serialises_with_camel_case_tag() {
        let payload = QrPayload::Phone { number: "+380991234567".into() };
        let json = serde_json::to_value(&payload).expect("serialize");
        assert_eq!(json["type"], "phone");
        assert_eq!(json["number"], "+380991234567");
    }

    #[test]
    fn vcard_payload_deserialises_from_front_end_shape() {
        let raw = r#"{"type":"vCard","firstName":"Олена","lastName":"К","phone":"+380"}"#;
        let payload: QrPayload = serde_json::from_str(raw).expect("deserialize");
        assert_eq!(payload.kind(), "vcard");
        assert_eq!(payload.display_label(), "Олена К");
    }
}
