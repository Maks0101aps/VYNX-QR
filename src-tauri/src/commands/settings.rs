//! Content analysis, logo loading and settings commands.

use std::path::Path;

use base64::Engine;
use base64::engine::general_purpose::STANDARD as BASE64;
use serde::{Deserialize, Serialize};

use crate::detect::Analysis;
use crate::error::{err_with, AppResult, ErrorCode};
use crate::platform::windows::{
    system_info as read_system_info, SystemInfo,
};
use crate::qr::logo::LogoInput;
use crate::settings::{self, Settings};

/// Classify the free-form input. Returns `null` for blank input.
#[tauri::command]
pub fn analyze_input(input: String) -> AppResult<Option<Analysis>> {
    crate::detect::analyze_input_checked(&input)
}

/// A logo that has been read from disk and is ready to be sent back to the core.
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
pub struct LogoAssetPayload {
    pub name: String,
    pub data: String,
    pub width: u32,
    pub height: u32,
}

fn build_logo_payload(name: String, bytes: &[u8]) -> AppResult<LogoAssetPayload> {
    let asset = crate::qr::logo::LogoAsset::from_bytes(name.clone(), bytes)?;
    Ok(LogoAssetPayload {
        name,
        data: BASE64.encode(bytes),
        width: asset.width,
        height: asset.height,
    })
}

/// Read a logo chosen through the native file picker.
#[tauri::command]
pub fn load_logo(path: String) -> AppResult<LogoAssetPayload> {
    let trimmed = path.trim();
    if trimmed.is_empty() {
        return Err(err_with(ErrorCode::LogoDecodeFailed, "empty path"));
    }
    let file = Path::new(trimmed);
    if !file.is_file() {
        return Err(err_with(ErrorCode::FileReadFailed, trimmed));
    }
    let metadata = std::fs::metadata(file)
        .map_err(|error| err_with(ErrorCode::FileReadFailed, error))?;
    if metadata.len() > crate::qr::logo::MAX_LOGO_BYTES as u64 {
        return Err(err_with(ErrorCode::LogoTooBig, metadata.len()));
    }
    let bytes =
        std::fs::read(file).map_err(|error| err_with(ErrorCode::FileReadFailed, error))?;
    let name = file
        .file_name()
        .map(|value| value.to_string_lossy().into_owned())
        .unwrap_or_else(|| "logo".to_string());
    build_logo_payload(name, &bytes)
}

/// Load the persisted preferences.
#[tauri::command]
pub fn load_settings() -> AppResult<Settings> {
    Ok(settings::load())
}

/// Persist the preferences.
#[tauri::command]
pub fn save_settings(settings: Settings) -> AppResult<()> {
    settings::save(&settings)
}

/// Host details for theming and the About dialog.
#[tauri::command]
pub fn system_info() -> AppResult<SystemInfo> {
    Ok(read_system_info())
}

/// True when the given payload can be encoded, used for inline form validation.
#[tauri::command]
pub fn validate_payload(payload: crate::qr::payload::QrPayload) -> AppResult<bool> {
    Ok(payload.encode().is_ok())
}

/// Convert a logo input into the shape the renderer expects.
#[tauri::command]
pub fn normalize_logo(input: LogoInput) -> AppResult<LogoInput> {
    let asset = crate::qr::logo::LogoAsset::decode(&input)?;
    Ok(LogoInput { name: asset.name, data: input.data })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::qr::payload::QrPayload;

    #[test]
    fn analysis_returns_none_for_blank_input() {
        assert!(analyze_input("   ".into()).expect("analysis").is_none());
    }

    #[test]
    fn analysis_flags_a_bare_domain() {
        let analysis = analyze_input("github.com".into())
            .expect("analysis")
            .expect("some analysis");
        assert!(analysis.normalization.is_some());
    }

    #[test]
    fn payload_validation_reports_encodability() {
        assert!(validate_payload(QrPayload::Text { text: "ok".into() }).expect("validate"));
        assert!(!validate_payload(QrPayload::Text { text: "  ".into() }).expect("validate"));
    }

    #[test]
    fn logo_loading_rejects_missing_files() {
        let error = load_logo("Z:\\missing\\logo.png".into()).unwrap_err();
        assert!(matches!(error.code(), ErrorCode::FileReadFailed | ErrorCode::LogoDecodeFailed));
    }
}
