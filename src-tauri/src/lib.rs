//! VYNX QR — fast, private QR codes for Windows.
//!
//! The crate is split so the QR engine can be tested without a window: payload
//! encoders, the matrix builder, the raster and vector renderers and the scan
//! verification are all plain Rust. Tauri only wires those pieces to the UI.

pub mod commands;
pub mod detect;
pub mod error;
pub mod formats;
pub mod platform;
pub mod qr;
pub mod settings;

use tauri::Manager;

use crate::qr::payload::QrPayload;

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    let builder = tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .setup(|app| {
            // Closing the window quits the process: no tray, no background work.
            if let Some(window) = app.get_webview_window("main") {
                let _ = window.set_focus();
            }
            Ok(())
        })
        .invoke_handler(tauri::generate_handler![
            commands::generate::render_qr,
            commands::generate::copy_qr,
            commands::generate::read_clipboard_text,
            commands::generate::export_qr,
            commands::settings::analyze_input,
            commands::settings::load_logo,
            commands::settings::load_settings,
            commands::settings::save_settings,
            commands::settings::system_info,
            commands::settings::validate_payload,
            commands::settings::normalize_logo,
        ]);

    builder
        .run(tauri::generate_context!())
        .expect("VYNX QR failed to start");
}

/// Re-exported for integration tests that want the pipeline without Tauri.
pub use crate::qr::{RenderRequest, QrStyle};

/// Convenience constructor used by tests and the CLI story of the crate.
pub fn request_for(text: &str) -> RenderRequest {
    RenderRequest {
        payload: QrPayload::Text { text: text.to_string() },
        style: QrStyle::default(),
        ec_level: crate::qr::payload::EcLevel::H,
        size_px: 512,
    }
}
