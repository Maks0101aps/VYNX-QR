//! Error type shared by every fallible operation in the engine.
//!
//! Every variant carries a stable machine readable `code` plus a sentence that is
//! safe to show verbatim in the user interface. Technical detail stays in the
//! `Display`/`Debug` representation so it can be printed to a developer console
//! without ever reaching the UI.

use std::fmt;

use serde::ser::{Serialize, SerializeStruct, Serializer};

/// Stable error codes. Keep in sync with `src/types/errors.ts`.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum ErrorCode {
    EmptyInput,
    InvalidUrl,
    InvalidEmail,
    InvalidPhone,
    InvalidLatitude,
    InvalidLongitude,
    InvalidWifi,
    InvalidColor,
    ContentTooLong,
    LogoTooLarge,
    LogoDecodeFailed,
    LogoTooBig,
    FileWriteFailed,
    FileReadFailed,
    ClipboardFailed,
    SettingsFailed,
    RenderFailed,
    VerificationFailed,
}

impl ErrorCode {
    pub const fn as_str(self) -> &'static str {
        match self {
            Self::EmptyInput => "emptyInput",
            Self::InvalidUrl => "invalidUrl",
            Self::InvalidEmail => "invalidEmail",
            Self::InvalidPhone => "invalidPhone",
            Self::InvalidLatitude => "invalidLatitude",
            Self::InvalidLongitude => "invalidLongitude",
            Self::InvalidWifi => "invalidWifi",
            Self::InvalidColor => "invalidColor",
            Self::ContentTooLong => "contentTooLong",
            Self::LogoTooLarge => "logoTooLarge",
            Self::LogoDecodeFailed => "logoDecodeFailed",
            Self::LogoTooBig => "logoTooBig",
            Self::FileWriteFailed => "fileWriteFailed",
            Self::FileReadFailed => "fileReadFailed",
            Self::ClipboardFailed => "clipboardFailed",
            Self::SettingsFailed => "settingsFailed",
            Self::RenderFailed => "renderFailed",
            Self::VerificationFailed => "verificationFailed",
        }
    }
}

/// Application error: user-facing message + internal detail.
#[derive(Debug)]
pub struct AppError {
    code: ErrorCode,
    message: &'static str,
    detail: String,
}

impl AppError {
    pub fn new(code: ErrorCode, message: &'static str) -> Self {
        Self {
            code,
            message,
            detail: String::new(),
        }
    }

    pub fn with_detail(mut self, detail: impl fmt::Display) -> Self {
        self.detail = detail.to_string();
        self
    }

    pub fn code(&self) -> ErrorCode {
        self.code
    }
}

impl fmt::Display for AppError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "[{}] {}", self.code.as_str(), self.message)?;
        if !self.detail.is_empty() {
            write!(f, " ({})", self.detail)?;
        }
        Ok(())
    }
}

impl std::error::Error for AppError {}

impl Serialize for AppError {
    fn serialize<S: Serializer>(&self, serializer: S) -> Result<S::Ok, S::Error> {
        let mut state = serializer.serialize_struct("AppError", 2)?;
        state.serialize_field("code", self.code.as_str())?;
        state.serialize_field("message", self.message)?;
        state.end()
    }
}

/// Ergonomic alias for command results.
pub type AppResult<T> = Result<T, AppError>;

pub mod messages {
    use super::ErrorCode;

    pub const EMPTY_INPUT: &str = "Enter something to create a QR code.";
    pub const INVALID_URL: &str = "This does not look like a valid web address.";
    pub const INVALID_EMAIL: &str = "This email address is not valid.";
    pub const INVALID_PHONE: &str = "This phone number is not valid.";
    pub const INVALID_LATITUDE: &str = "Latitude must be a number between -90 and 90.";
    pub const INVALID_LONGITUDE: &str = "Longitude must be a number between -180 and 180.";
    pub const INVALID_WIFI: &str = "Enter a network name (SSID).";
    pub const INVALID_COLOR: &str = "Use a HEX colour such as #1A1A1C.";
    pub const CONTENT_TOO_LONG: &str =
        "This content is too large for a QR code with the current settings.";
    pub const LOGO_TOO_LARGE: &str = "Logo is too large for reliable scanning.";
    pub const LOGO_DECODE_FAILED: &str = "The selected image could not be loaded.";
    pub const LOGO_TOO_BIG: &str = "That image is too large. Use an image under 4096 x 4096.";
    pub const FILE_WRITE_FAILED: &str = "VYNX QR could not save the file.";
    pub const FILE_READ_FAILED: &str = "VYNX QR could not read that file.";
    pub const CLIPBOARD_FAILED: &str = "VYNX QR could not access the Windows clipboard.";
    pub const SETTINGS_FAILED: &str = "VYNX QR could not load your settings.";
    pub const RENDER_FAILED: &str = "VYNX QR could not render this QR code.";
    pub const VERIFICATION_FAILED: &str = "QR verification failed.";

    pub const fn for_code(code: ErrorCode) -> &'static str {
        match code {
            ErrorCode::EmptyInput => EMPTY_INPUT,
            ErrorCode::InvalidUrl => INVALID_URL,
            ErrorCode::InvalidEmail => INVALID_EMAIL,
            ErrorCode::InvalidPhone => INVALID_PHONE,
            ErrorCode::InvalidLatitude => INVALID_LATITUDE,
            ErrorCode::InvalidLongitude => INVALID_LONGITUDE,
            ErrorCode::InvalidWifi => INVALID_WIFI,
            ErrorCode::InvalidColor => INVALID_COLOR,
            ErrorCode::ContentTooLong => CONTENT_TOO_LONG,
            ErrorCode::LogoTooLarge => LOGO_TOO_LARGE,
            ErrorCode::LogoDecodeFailed => LOGO_DECODE_FAILED,
            ErrorCode::LogoTooBig => LOGO_TOO_BIG,
            ErrorCode::FileWriteFailed => FILE_WRITE_FAILED,
            ErrorCode::FileReadFailed => FILE_READ_FAILED,
            ErrorCode::ClipboardFailed => CLIPBOARD_FAILED,
            ErrorCode::SettingsFailed => SETTINGS_FAILED,
            ErrorCode::RenderFailed => RENDER_FAILED,
            ErrorCode::VerificationFailed => VERIFICATION_FAILED,
        }
    }
}

/// Build an [`AppError`] from just a code, using the canonical message.
pub fn err(code: ErrorCode) -> AppError {
    AppError::new(code, messages::for_code(code))
}

pub fn err_with(code: ErrorCode, detail: impl fmt::Display) -> AppError {
    AppError::new(code, messages::for_code(code)).with_detail(detail)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn error_serializes_to_code_and_message() {
        let error = err_with(ErrorCode::InvalidUrl, "no host");
        let json = serde_json::to_value(&error).expect("serialize");
        assert_eq!(json["code"], "invalidUrl");
        assert_eq!(json["message"], messages::INVALID_URL);
        assert!(json.get("detail").is_none());
    }

    #[test]
    fn display_keeps_detail_for_developer_console() {
        let error = err_with(ErrorCode::FileWriteFailed, "access denied");
        assert_eq!(
            error.to_string(),
            "[fileWriteFailed] VYNX QR could not save the file. (access denied)"
        );
    }
}
