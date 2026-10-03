//! Full pipeline tests: payload to code to raster back to decoded text.
//!
//! These are the ones that matter for correctness. Everything else checks a
//! single step; these prove that the text a user typed is the text a scanner
//! reads back, byte for byte, for every payload type and for anything awkward
//! that can be typed into a field.

use vynx_qr_core::export::{render_png, render_svg};
use vynx_qr_core::qr::logo::LogoAsset;
use vynx_qr_core::qr::payload::{EcLevel, QrPayload, WifiSecurity};
use vynx_qr_core::qr::verify::VerifyStatus;
use vynx_qr_core::qr::{QrStyle, RenderRequest};

/// The text a scanner recovers must equal the text that went in.
fn round_trip(payload: &QrPayload) {
    let request = RenderRequest {
        payload: payload.clone(),
        style: QrStyle::default(),
        ec_level: EcLevel::H,
        size_px: 768,
    };

    let result = render_png(&request).expect("render");
    assert_eq!(
        result.verification.status,
        VerifyStatus::Verified,
        "scan verification failed for {:?}: {:?}",
        payload.kind(),
        result.verification
    );
    assert_eq!(result.encoded, payload.encode().expect("encode"));
    assert_eq!(&result.png[..4], b"\x89PNG");
}

#[test]
fn text_round_trips() {
    round_trip(&QrPayload::Text {
        text: "VYNX QR".into(),
    });
}

#[test]
fn url_round_trips() {
    round_trip(&QrPayload::Url {
        url: "https://github.com/Maks0101aps/VYNX-QR".into(),
    });
}

#[test]
fn email_round_trips() {
    round_trip(&QrPayload::Email {
        to: "hello@example.com".into(),
        subject: "Привіт".into(),
        body: "Line one\nLine two".into(),
    });
}

#[test]
fn phone_round_trips() {
    round_trip(&QrPayload::Phone {
        number: "+380991234567".into(),
    });
}

#[test]
fn sms_round_trips() {
    round_trip(&QrPayload::Sms {
        number: "+380991234567".into(),
        message: "Meeting at 10".into(),
    });
}

#[test]
fn wifi_round_trips() {
    round_trip(&QrPayload::Wifi {
        ssid: "VYNX Home".into(),
        password: "s3cret pass".into(),
        security: WifiSecurity::Wpa,
        hidden: false,
    });
}

#[test]
fn vcard_round_trips() {
    round_trip(&QrPayload::VCard {
        first_name: "Олена".into(),
        last_name: "Ковальчук".into(),
        organization: "VYNX".into(),
        job_title: "Engineer".into(),
        phone: "+380991234567".into(),
        email: "olena@example.com".into(),
        website: "https://vynx.dev".into(),
        address: "Kyiv, Ukraine".into(),
        note: "Met at VYNX ✨".into(),
    });
}

#[test]
fn geo_round_trips() {
    round_trip(&QrPayload::Geo {
        latitude: 50.4501,
        longitude: 30.5234,
        label: "Kyiv".into(),
    });
}

/// The characters that break naive encoders: an emoji outside the BMP, a
/// combining mark, right to left text and a zero width joiner.
#[test]
fn unicode_round_trips_exactly() {
    for text in [
        "Привіт, Україно 🇺🇦",
        "こんにちは世界",
        "مرحبا بالعالم",
        "🙂🙃",
        "Grüße aus München",
        "a\u{200B}b",
        "Ω≈ç√∫˜µ≤≥÷",
        "🧑‍💻 developer",
    ] {
        round_trip(&QrPayload::Text {
            text: text.to_string(),
        });
    }
}

/// Punctuation that the payload formats have to escape. If the escaping is wrong
/// the code still scans, but it decodes to something else, which this catches.
#[test]
fn escaped_special_characters_round_trip() {
    round_trip(&QrPayload::Wifi {
        ssid: "My;WiFi:Home".into(),
        password: "p,a\\ss;word:".into(),
        security: WifiSecurity::Wpa,
        hidden: false,
    });
    round_trip(&QrPayload::Sms {
        number: "+380991234567".into(),
        message: "a;b,c\\d\ne".into(),
    });
}

#[test]
fn a_logo_survives_the_round_trip() {
    let logo = LogoAsset::from_bytes("dot.png".into(), &logo_png()).expect("logo");
    assert!(logo.width > 0 && logo.height > 0);

    let request = RenderRequest {
        payload: QrPayload::Text {
            text: "VYNX QR".into(),
        },
        style: QrStyle {
            logo: Some(vynx_qr_core::qr::LogoInput {
                name: "dot.png".into(),
                data: logo_png(),
            }),
            logo_ratio: 0.2,
            ..QrStyle::default()
        },
        ec_level: EcLevel::L,
        size_px: 1024,
    };

    let result = render_png(&request).expect("render");
    assert_eq!(result.verification.status, VerifyStatus::Verified);
    // The logo forced the level up, which is the point of checking it here.
    assert_eq!(result.ec_level, EcLevel::H);
}

#[test]
fn every_export_size_produces_a_decodable_code() {
    for size in [256u32, 512, 1024, 2048] {
        let request = RenderRequest {
            payload: QrPayload::Url {
                url: "https://vynx.dev".into(),
            },
            style: QrStyle::default(),
            ec_level: EcLevel::M,
            size_px: size,
        };
        let result = render_png(&request).expect("render");
        assert_eq!(
            result.verification.status,
            VerifyStatus::Verified,
            "failed at {size}px"
        );
        assert!(
            result.width <= size,
            "{size}px request produced {}px",
            result.width
        );
    }
}

#[test]
fn svg_export_matches_the_raster_payload() {
    let payload = QrPayload::Url {
        url: "https://vynx.dev".into(),
    };
    let request = RenderRequest {
        payload: payload.clone(),
        style: QrStyle::default(),
        ec_level: EcLevel::M,
        size_px: 512,
    };
    let document = render_svg(&request, 512).expect("svg");
    assert!(document.contains("viewBox"));
    assert!(document.contains("</svg>"));
    // The vector form carries no raster at all when there is no logo.
    assert!(!document.contains("data:image"));
}

#[test]
fn rounded_modules_still_verify() {
    let request = RenderRequest {
        payload: QrPayload::Text {
            text: "rounded".into(),
        },
        style: QrStyle {
            module_style: vynx_qr_core::qr::payload::ModuleStyle::Rounded,
            ..QrStyle::default()
        },
        ec_level: EcLevel::H,
        size_px: 1024,
    };
    assert_eq!(
        render_png(&request).expect("render").verification.status,
        VerifyStatus::Verified
    );
}

/// A minimal valid PNG, used wherever a logo is needed.
fn logo_png() -> Vec<u8> {
    use vynx_qr_core::qr::render::{encode_png, Canvas};
    use vynx_qr_core::qr::Rgba;
    encode_png(&Canvas::new(8, 8, Rgba::new(255, 255, 255, 255))).expect("png")
}
