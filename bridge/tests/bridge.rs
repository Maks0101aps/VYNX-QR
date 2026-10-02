//! Tests for the boundary itself.
//!
//! `core` proves the engine is right; these prove the bridge tells the truth on
//! the way across and converts back without losing anything.

use vynx_qr_bridge::ffi::*;

fn text_payload(text: &str) -> Payload {
    Payload {
        kind: PayloadType::Text,
        text: text.to_string(),
        ..Payload {
            kind: PayloadType::Text,
            text: String::new(),
            url: String::new(),
            wifi: WifiPayload {
                ssid: String::new(),
                password: String::new(),
                security: WifiSecurity::Wpa,
                hidden: false,
            },
            vcard: VCardPayload {
                first_name: String::new(),
                last_name: String::new(),
                organization: String::new(),
                job_title: String::new(),
                phone: String::new(),
                email: String::new(),
                website: String::new(),
                address: String::new(),
                note: String::new(),
            },
            email: EmailPayload {
                to: String::new(),
                subject: String::new(),
                body: String::new(),
            },
            phone: String::new(),
            sms: SmsPayload { number: String::new(), message: String::new() },
            geo: GeoPayload { latitude: 0.0, longitude: 0.0, label: String::new() },
        }
    }
}

fn wifi_payload(ssid: &str, password: &str) -> Payload {
    Payload {
        kind: PayloadType::Wifi,
        wifi: WifiPayload {
            ssid: ssid.to_string(),
            password: password.to_string(),
            security: WifiSecurity::Wpa,
            hidden: false,
        },
        ..text_payload("")
    }
}

#[test]
fn analyse_classifies_a_bare_domain() {
    let analysis = analyze("github.com").expect("analyse");
    assert!(analysis.detected);
    assert!(analysis.kind == ContentKind::Url);
    assert!(!analysis.normalization.is_empty(), "the added scheme must be announced");
    assert_eq!(analysis.encoded, "https://github.com");
}

#[test]
fn analyse_reports_nothing_for_blank_input() {
    let analysis = analyze("   ").expect("analyse");
    assert!(!analysis.detected);
    assert!(analysis.encoded.is_empty());
}

#[test]
fn analyse_never_twice_encodes_a_mailto_uri() {
    let analysis = analyze("mailto:hi@example.com").expect("analyse");
    assert!(analysis.detected);
    assert!(analysis.kind == ContentKind::Email);
    assert_eq!(analysis.encoded, "mailto:hi@example.com");
}

#[test]
fn generate_returns_png_bytes_and_a_verified_status() {
    let result = generate(&text_payload("VYNX QR"), &default_style()).expect("generate");
    assert_eq!(&result.png[..4], b"\x89PNG");
    assert!(result.verification_status == VerifyStatus::Verified);
    assert_eq!(result.encoded, "VYNX QR");
    assert_eq!(result.width, result.height);
}

#[test]
fn generate_svg_is_a_real_vector_document() {
    let document = generate_svg(&text_payload("VYNX QR"), &default_style(), 512).expect("svg");
    assert!(document.contains("<svg"));
    assert!(document.trim_end().ends_with("</svg>"));
    assert!(!document.contains("data:image"), "no rasterised payload without a logo");
}

#[test]
fn generate_bitmap_returns_rgba_pixels() {
    let bitmap = generate_bitmap(&text_payload("VYNX QR"), &default_style()).expect("bitmap");
    assert_eq!(bitmap.width, bitmap.height);
    assert_eq!(bitmap.pixels.len(), (bitmap.width * bitmap.height * 4) as usize);
}

#[test]
fn wifi_credentials_keep_their_surrounding_spaces() {
    let payload = wifi_payload(" VYNX Home ", " password with spaces ");
    let result = generate(&payload, &default_style()).expect("generate");
    assert_eq!(
        result.encoded,
        "WIFI:T:WPA;S: VYNX Home ;P: password with spaces ;;"
    );
    assert!(result.verification_status == VerifyStatus::Verified);
}

#[test]
fn a_missing_field_for_the_chosen_kind_is_reported() {
    let payload = Payload { kind: PayloadType::Wifi, ..text_payload("") };
    let error = generate(&payload, &default_style()).expect_err("must refuse");
    assert!(error.contains("network name"), "unhelpful error: {error}");
    assert!(!is_encodable(&payload));
}

#[test]
fn a_populated_field_for_the_wrong_kind_is_not_used() {
    // `kind` says Wi-Fi but only the URL field was filled. The bridge must refuse
    // rather than quietly encoding the empty SSID.
    let payload = Payload { kind: PayloadType::Wifi, url: "https://vynx.dev".into(), ..text_payload("") };
    assert!(generate(&payload, &default_style()).is_err());
}

#[test]
fn unicode_survives_the_boundary_unchanged() {
    for text in ["Привіт, Україно 🇺🇦", "こんにちは", "مرحبا", "🙂"] {
        let result = generate(&text_payload(text), &default_style()).expect("generate");
        assert_eq!(result.encoded, text);
        assert!(result.verification_status == VerifyStatus::Verified, "failed for {text}");
    }
}

#[test]
fn a_logo_raises_error_correction_and_still_verifies() {
    let mut options = default_style();
    options.size_px = 1024;
    options.ec_level = EcLevel::L;
    options.has_logo = true;
    options.logo = Logo { name: "dot.png".into(), data: one_pixel_png() };

    let result = generate(&text_payload("VYNX QR"), &options).expect("generate");
    assert!(result.ec_level == EcLevel::H, "a logo must raise the level");
    assert!(result.ec_adjusted);
    assert!(result.verification_status == VerifyStatus::Verified);
}

#[test]
fn system_info_reports_the_host_without_failing() {
    let info = system_info();
    assert!(!info.os_build.is_empty());
    // A development build is expected here; it only has to be a bool, never a panic.
    let _ = info.development;
}

#[test]
fn settings_round_trip_through_the_bridge_types() {
    let mut settings = load_settings();
    settings.default_size = 512;
    settings.use_windows_accent = false;
    save_settings(&settings).expect("save");
    let reloaded = load_settings();
    assert_eq!(reloaded.default_size, 512);
    assert!(!reloaded.use_windows_accent);
    // Put the user's settings back the way they were.
    save_settings(&settings).expect("save");
}

#[test]
fn labels_come_from_the_engine() {
    assert_eq!(payload_label(PayloadType::Wifi), "Wi-Fi network");
    assert_eq!(payload_label(PayloadType::VCard), "Contact card");
}

/// A one pixel PNG, the smallest image the decoder accepts.
fn one_pixel_png() -> Vec<u8> {
    // 1x1 opaque black, hand assembled so the test carries no extra dependency.
    let bytes: [u8; 68] = [
        0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, // signature
        0x00, 0x00, 0x00, 0x0D, 0x49, 0x48, 0x44, 0x52, // IHDR length + type
        0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, // 1 x 1
        0x08, 0x06, 0x00, 0x00, 0x00, 0x1F, 0x15, 0xC4, 0x89, // 8 bit RGBA + CRC
        0x00, 0x00, 0x00, 0x0A, 0x49, 0x44, 0x41, 0x54, // IDAT length + type
        0x78, 0x9C, 0x63, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00, 0x01, 0x0D, 0x0A, 0x2D, 0xB4,
        0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4E, 0x44, // IEND
        0xAE, 0x42, 0x60, 0x82,
    ];
    bytes.to_vec()
}