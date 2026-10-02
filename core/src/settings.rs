//! Persisted preferences.
//!
//! Only preferences are stored, never QR content. The file is a small JSON
//! document in `%APPDATA%\VYNX\QR\settings.json`; no database, no sync, no
//! telemetry.

use std::fs;
use std::path::{Path, PathBuf};

use serde::{Deserialize, Serialize};

use crate::error::{err_with, AppResult, ErrorCode};
use crate::qr::payload::{EcLevel, ModuleStyle};

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize, Default)]
#[serde(rename_all = "camelCase")]
pub enum ThemeMode {
    System,
    #[default]
    Light,
    Dark,
}

/// Default export file format.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize, Default)]
#[serde(rename_all = "camelCase")]
pub enum ExportFormat {
    #[default]
    Png,
    Svg,
}

/// Missing fields fall back to the defaults, so a settings file written by an
/// older build still loads.
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "camelCase", default)]
pub struct Settings {
    pub theme: ThemeMode,
    /// Follow the Windows accent colour instead of the VYNX blue.
    pub use_windows_accent: bool,
    /// Read the clipboard once at start-up and offer it as a suggestion.
    pub clipboard_check: bool,
    /// Use clipboard text immediately instead of asking.
    pub auto_paste: bool,
    pub default_format: ExportFormat,
    pub default_size: u32,
    pub default_error_correction: EcLevel,
    pub default_module_style: ModuleStyle,
}

impl Default for Settings {
    fn default() -> Self {
        Self {
            theme: ThemeMode::System,
            use_windows_accent: true,
            clipboard_check: true,
            auto_paste: false,
            default_format: ExportFormat::Png,
            default_size: 1024,
            // High keeps every payload type comfortably scannable, including
            // vCards and Wi-Fi credentials, at a modest size cost.
            default_error_correction: EcLevel::H,
            default_module_style: ModuleStyle::Square,
        }
    }
}

impl Settings {
    /// Clamp values that could produce an unusable export.
    pub fn sanitized(mut self) -> Self {
        if !crate::qr::EXPORT_SIZES.contains(&self.default_size) {
            self.default_size = 1024;
        }
        self
    }
}

/// Where preferences live: `%APPDATA%\VYNX\QR\settings.json`.
///
/// Exposed so a test, or a portable build, can reason about the location without
/// having to guess at the environment.
pub fn default_path() -> Option<PathBuf> {
    let appdata = std::env::var_os("APPDATA")?;
    Some(PathBuf::from(appdata).join("VYNX").join("QR").join("settings.json"))
}

/// Read settings from disk, falling back to defaults when the file is missing or
/// unreadable. A corrupt preferences file must never stop the app from starting.
pub fn load() -> Settings {
    match default_path() {
        Some(path) => load_from(&path),
        None => Settings::default(),
    }
}

/// Read settings from an explicit path.
///
/// Taking the path as an argument rather than reading a global is what lets tests
/// use a temporary file: a test must never load or write the real user's
/// preferences, and a global override would be shared across the threads a test
/// binary runs on.
pub fn load_from(path: &Path) -> Settings {
    let Ok(text) = fs::read_to_string(path) else {
        return Settings::default();
    };
    match serde_json::from_str::<Settings>(&text) {
        Ok(settings) => settings.sanitized(),
        Err(_) => Settings::default(),
    }
}

/// Persist settings atomically enough for a desktop utility.
pub fn save(settings: &Settings) -> AppResult<()> {
    let Some(path) = default_path() else {
        return Err(err_with(ErrorCode::SettingsFailed, "APPDATA is not set"));
    };
    save_to(&path, settings)
}

/// Persist settings to an explicit path. See [`load_from`] on why this exists.
pub fn save_to(path: &Path, settings: &Settings) -> AppResult<()> {
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent)
            .map_err(|error| err_with(ErrorCode::SettingsFailed, error))?;
    }
    let json = serde_json::to_string_pretty(settings)
        .map_err(|error| err_with(ErrorCode::SettingsFailed, error))?;
    fs::write(path, json).map_err(|error| err_with(ErrorCode::SettingsFailed, error))?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn defaults_match_the_documented_product_decisions() {
        let settings = Settings::default();
        assert_eq!(settings.theme, ThemeMode::System);
        assert_eq!(settings.default_size, 1024);
        assert_eq!(settings.default_error_correction, EcLevel::H);
        assert!(settings.clipboard_check);
        assert!(!settings.auto_paste);
        assert_eq!(settings.default_format, ExportFormat::Png);
    }

    #[test]
    fn round_trips_through_json() {
        let settings = Settings { theme: ThemeMode::Dark, default_size: 512, ..Settings::default() };
        let json = serde_json::to_string(&settings).expect("serialize");
        let restored: Settings = serde_json::from_str(&json).expect("deserialize");
        assert_eq!(restored, settings);
    }

    #[test]
    fn unknown_future_fields_do_not_break_loading() {
        let json = r#"{"theme":"dark","somethingNew":42}"#;
        let parsed: Settings = serde_json::from_str(json).expect("deserialize");
        assert_eq!(parsed.theme, ThemeMode::Dark);
    }

    #[test]
    fn sanitising_rejects_out_of_range_sizes() {
        let settings = Settings { default_size: 37, ..Settings::default() };
        assert_eq!(settings.sanitized().default_size, 1024);
        let ok = Settings { default_size: 2048, ..Settings::default() };
        assert_eq!(ok.sanitized().default_size, 2048);
    }

    #[test]
    fn saves_and_loads_through_an_explicit_path() {
        let dir = tempfile::tempdir().expect("tempdir");
        let path = dir.path().join("nested").join("settings.json");
        let settings = Settings { theme: ThemeMode::Dark, default_size: 512, ..Settings::default() };

        save_to(&path, &settings).expect("save");
        assert!(path.is_file(), "the parent directory must be created");

        let loaded = load_from(&path);
        assert_eq!(loaded.theme, ThemeMode::Dark);
        assert_eq!(loaded.default_size, 512);
    }

    #[test]
    fn a_missing_file_yields_defaults_rather_than_an_error() {
        let dir = tempfile::tempdir().expect("tempdir");
        let loaded = load_from(&dir.path().join("absent.json"));
        assert_eq!(loaded, Settings::default());
    }

    #[test]
    fn a_corrupt_file_yields_defaults_rather_than_an_error() {
        let dir = tempfile::tempdir().expect("tempdir");
        let path = dir.path().join("settings.json");
        fs::write(&path, "{ this is not json").expect("write");
        assert_eq!(load_from(&path), Settings::default());
    }

    #[test]
    fn an_out_of_range_size_is_repaired_on_load() {
        let dir = tempfile::tempdir().expect("tempdir");
        let path = dir.path().join("settings.json");
        fs::write(&path, r#"{"defaultSize":37}"#).expect("write");
        assert_eq!(load_from(&path).default_size, 1024);
    }

    #[test]
    fn the_default_path_is_the_documented_one() {
        // Only meaningful where APPDATA exists, which is the platform we ship on.
        if let Some(path) = default_path() {
            let text = path.to_string_lossy().replace('/', "\\");
            assert!(text.ends_with("VYNX\\QR\\settings.json"), "unexpected path: {text}");
        }
    }
}
