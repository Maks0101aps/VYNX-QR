//! CXX bridge: the only surface C++ can see.
//!
//! Everything here is a typed data transfer object plus a thin function that
//! forwards to `vynx_qr_core`. There is no JSON, no serialisation by hand and no
//! untyped map anywhere on the path, so a shape mismatch is a compile error in both
//! languages rather than a runtime surprise.
//!
//! # Why the payload is a tagged struct
//!
//! `cxx` supports neither enums with data nor `std::variant`, so a Rust enum
//! cannot cross the boundary. The alternative would have been eight separate
//! `generate_*` entry points per output format, twenty four functions to keep in
//! step.
//!
//! Instead `Payload` carries a `kind` tag plus one named sub-struct per kind. It
//! is still typed: every field has a declared type and both compilers check it.
//! The one thing a tag can get wrong, populating the wrong sub-struct, is caught
//! here rather than silently ignored, because `to_core_payload` refuses a payload
//! whose chosen field is empty.
//!
//! # Rules at this boundary
//!
//! * No panic may cross it. Every entry point returns `Result`, and the error
//!   carries a code and a message that are safe to show a user.
//! * No UI state goes through it. The engine is handed a payload and a style and
//!   returns bytes and facts; what to do with them is the caller's problem.
//! * Strings are UTF-8 in both directions, exactly as typed.

use std::panic::{catch_unwind, AssertUnwindSafe};

#[cxx::bridge(namespace = "vynx")]
pub mod ffi {
    /// Which kind of payload this is, for the status line and file naming.
    enum PayloadType {
        Text,
        Url,
        Wifi,
        VCard,
        Email,
        Phone,
        Sms,
        Geo,
    }

    enum ModuleStyle {
        Square,
        Rounded,
    }

    enum EcLevel {
        L,
        M,
        Q,
        H,
    }

    enum WifiSecurity {
        Wpa,
        Wep,
        None,
    }

    /// What the detector concluded about free-form input.
    enum ContentKind {
        Text,
        Url,
        Email,
        Phone,
    }

    /// What actually happened when the rendered image was decoded again.
    enum VerifyStatus {
        Verified,
        Failed,
        Mismatch,
    }

    enum ExportFormat {
        Png,
        Svg,
    }

    enum ThemeMode {
        System,
        Light,
        Dark,
    }

    struct WifiPayload {
        /// Kept exactly as typed, including surrounding spaces: those are part of
        /// the network name.
        ssid: String,
        password: String,
        security: WifiSecurity,
        hidden: bool,
    }

    struct VCardPayload {
        first_name: String,
        last_name: String,
        organization: String,
        job_title: String,
        phone: String,
        email: String,
        website: String,
        address: String,
        note: String,
    }

    struct EmailPayload {
        to: String,
        subject: String,
        body: String,
    }

    struct SmsPayload {
        number: String,
        message: String,
    }

    struct GeoPayload {
        latitude: f64,
        longitude: f64,
        label: String,
    }

    /// What the user wants encoded.
    ///
    /// `kind` selects which field is meaningful. Filling a field that `kind` does
    /// not select is harmless; selecting a kind and leaving its field blank is
    /// refused with a clear error.
    struct Payload {
        kind: PayloadType,
        /// Used when `kind` is Text.
        text: String,
        /// Used when `kind` is Url.
        url: String,
        wifi: WifiPayload,
        vcard: VCardPayload,
        email: EmailPayload,
        /// Used when `kind` is Phone.
        phone: String,
        sms: SmsPayload,
        geo: GeoPayload,
    }

    /// Logo bytes as picked from disk or dropped onto the window.
    struct Logo {
        name: String,
        /// PNG, JPEG or WebP bytes, exactly as read.
        data: Vec<u8>,
    }

    /// A logo after the engine has validated it.
    struct LogoAsset {
        name: String,
        width: u32,
        height: u32,
    }

    /// Everything the window needs to render a preview in one round trip.
    struct RenderOptions {
        size_px: u32,
        ec_level: EcLevel,
        module_style: ModuleStyle,
        /// `#RRGGBB`
        foreground: String,
        /// `#RRGGBB`
        background: String,
        quiet_zone: u32,
        /// Fraction of the width the logo covers, 0.05 to 0.30.
        logo_ratio: f32,
        /// True when `logo` should be used. cxx has no `Option` for a shared
        /// struct, so presence is explicit rather than implied by an empty name.
        has_logo: bool,
        /// Meaningful only when `has_logo` is true.
        logo: Logo,
    }

    struct Warning {
        code: String,
        message: String,
    }

    struct GenerateResult {
        /// Raw PNG bytes. Build a QImage straight from this buffer.
        png: Vec<u8>,
        width: u32,
        height: u32,
        /// Modules per side, excluding the quiet zone.
        modules: u32,
        total_modules: u32,
        quiet_zone: u32,
        /// QR version, 1 to 40.
        version: u32,
        /// Level actually used, after the automatic logo bump.
        ec_level: EcLevel,
        ec_adjusted: bool,
        /// WCAG contrast ratio between the two colours.
        contrast: f32,
        verification_status: VerifyStatus,
        /// True when the reduced size re-check ran, which it only does with a logo.
        has_reduced_verification: bool,
        /// The reduced size result. Meaningful only when the flag above is true.
        reduced_verification: VerifyStatus,
        /// Whatever the decoder recovered instead, empty on success.
        decoded: String,
        warnings: Vec<Warning>,
        /// The exact string stored in the symbol.
        encoded: String,
    }

    /// Classification of free-form input.
    ///
    /// `normalization` and `notice` are empty strings when they do not apply, so
    /// C++ never has to model an absent optional string.
    struct Analysis {
        /// False when the input was blank and nothing was detected.
        detected: bool,
        kind: ContentKind,
        original: String,
        encoded: String,
        normalization: String,
        notice: String,
        /// True when the encoded form differs from what was typed, which is the
        /// only case where the window should offer to keep the original.
        changes_input: bool,
    }

    /// What the detector decided, together with the payload to actually encode.
    ///
    /// The payload is produced by the engine rather than rebuilt from
    /// `Analysis::encoded`. That distinction matters: `encoded` is the finished
    /// string that goes into the symbol, so for an email address it already
    /// carries `mailto:`. Handing that string back to the email encoder produces
    /// `mailto:mailto:hello@example.com`.
    struct SmartPayload {
        /// False when the input was blank. `payload` is then an empty text payload,
        /// which cxx requires because it cannot express an absent shared struct.
        detected: bool,
        analysis: Analysis,
        payload: Payload,
    }

    /// Straight RGBA pixels, for the clipboard.
    struct Bitmap {
        width: u32,
        height: u32,
        /// RGBA8, row major, four bytes per pixel.
        pixels: Vec<u8>,
    }

    struct SystemInfo {
        os_build: String,
        is_windows_11: bool,
        /// `#RRGGBB`, empty when Windows reports no custom accent.
        accent_color: String,
        development: bool,
    }

    struct Settings {
        theme: ThemeMode,
        use_windows_accent: bool,
        clipboard_check: bool,
        auto_paste: bool,
        default_format: ExportFormat,
        default_size: u32,
        default_error_correction: EcLevel,
        default_module_style: ModuleStyle,
    }

    extern "Rust" {
        /// Classify free-form input. Check `detected` on the result rather than
        /// expecting an optional, because cxx cannot return one of a shared struct.
        fn analyze(input: &str) -> Result<Analysis>;

        /// Classify free-form input and get the payload to encode in one call.
        ///
        /// The two travel together on purpose. Classification is only useful to the UI
        /// because it also decides what gets encoded, and keeping them apart invites the
        /// window to reassemble a payload from the encoded string, which is the one thing
        /// it must never do.
        fn smart_payload(input: &str) -> Result<SmartPayload>;

        /// The "keep exactly what I typed" alternative to a detected URL.
        ///
        /// Offered only when the detector actually changed something, so this is the
        /// escape hatch for normalisation rather than a second encoding path.
        fn original_text_payload(input: &str) -> Payload;

        /// Render a preview: encode, build, rasterise, verify.
        fn generate(payload: &Payload, options: &RenderOptions) -> Result<GenerateResult>;

        /// Render the vector form.
        fn generate_svg(payload: &Payload, options: &RenderOptions, size_px: u32)
            -> Result<String>;

        /// Render straight to RGBA, for the clipboard.
        fn generate_bitmap(payload: &Payload, options: &RenderOptions) -> Result<Bitmap>;

        /// True when the payload can be encoded, for inline validation.
        fn is_encodable(payload: &Payload) -> bool;

        /// A human readable label for a payload kind, for the status line.
        fn payload_label(kind: PayloadType) -> String;

        /// Read and validate a logo file.
        fn load_logo(path: &str) -> Result<LogoAsset>;

        fn system_info() -> SystemInfo;

        fn load_settings() -> Settings;
        fn save_settings(settings: &Settings) -> Result<()>;

        /// Style defaults, so the window starts from the engine's own idea of them.
        fn default_style() -> RenderOptions;
    }
}

use ffi::{
    Analysis, Bitmap, ContentKind, EcLevel, EmailPayload, ExportFormat, GenerateResult, GeoPayload,
    Logo, LogoAsset, ModuleStyle, Payload, PayloadType, RenderOptions, Settings, SmartPayload,
    SmsPayload, SystemInfo, ThemeMode, VCardPayload, VerifyStatus, Warning, WifiPayload,
    WifiSecurity,
};

/// Wrap a core call so that a panic becomes an error instead of unwinding into
/// C++.
///
/// A panic that crosses the FFI boundary is undefined behaviour. Every fallible
/// entry point goes through here, so the worst case is a reported error.
fn guard<T>(what: &str, body: impl FnOnce() -> Result<T, String>) -> Result<T, String> {
    match catch_unwind(AssertUnwindSafe(body)) {
        Ok(result) => result,
        Err(_) => Err(format!("{what} failed unexpectedly")),
    }
}

/// Build the engine payload, refusing a tag whose own field was left blank.
///
/// Without this a caller could set `kind` to Wifi while filling in `url`, and the
/// result would be a QR code of an empty SSID with no warning.
fn to_core_payload(value: &Payload) -> Result<vynx_qr_core::qr::payload::QrPayload, String> {
    use vynx_qr_core::qr::payload::QrPayload;

    let missing = |field: &str| Err(format!("Input required: {field}"));

    // Compared rather than matched, because a cxx shared enum carries a `repr`
    // byte and cannot be matched exhaustively. The final `else` is Geo, which
    // needs no mandatory field.
    Ok(if value.kind == PayloadType::Text {
        if value.text.is_empty() {
            return missing("text");
        }
        QrPayload::Text {
            text: value.text.clone(),
        }
    } else if value.kind == PayloadType::Url {
        if value.url.is_empty() {
            return missing("url");
        }
        QrPayload::Url {
            url: value.url.clone(),
        }
    } else if value.kind == PayloadType::Wifi {
        if value.wifi.ssid.is_empty() {
            return missing("network name");
        }
        QrPayload::Wifi {
            ssid: value.wifi.ssid.clone(),
            password: value.wifi.password.clone(),
            security: core_wifi_security(value.wifi.security),
            hidden: value.wifi.hidden,
        }
    } else if value.kind == PayloadType::VCard {
        QrPayload::VCard {
            first_name: value.vcard.first_name.clone(),
            last_name: value.vcard.last_name.clone(),
            organization: value.vcard.organization.clone(),
            job_title: value.vcard.job_title.clone(),
            phone: value.vcard.phone.clone(),
            email: value.vcard.email.clone(),
            website: value.vcard.website.clone(),
            address: value.vcard.address.clone(),
            note: value.vcard.note.clone(),
        }
    } else if value.kind == PayloadType::Email {
        if value.email.to.is_empty() {
            return missing("recipient");
        }
        QrPayload::Email {
            to: value.email.to.clone(),
            subject: value.email.subject.clone(),
            body: value.email.body.clone(),
        }
    } else if value.kind == PayloadType::Phone {
        if value.phone.is_empty() {
            return missing("number");
        }
        QrPayload::Phone {
            number: value.phone.clone(),
        }
    } else if value.kind == PayloadType::Sms {
        if value.sms.number.is_empty() {
            return missing("number");
        }
        QrPayload::Sms {
            number: value.sms.number.clone(),
            message: value.sms.message.clone(),
        }
    } else if value.kind == PayloadType::Geo {
        QrPayload::Geo {
            latitude: value.geo.latitude,
            longitude: value.geo.longitude,
            label: value.geo.label.clone(),
        }
    } else {
        // A cxx shared enum carries a `repr` byte, so a caller built against a
        // newer header can send a kind this build has never heard of. Falling back
        // to Geo here would encode a pin at (0, 0) with no warning, which is worse
        // than refusing: the user would scan their own code and get a map of the
        // Atlantic. Refusing is the only safe direction.
        return Err(format!(
            "Unsupported payload type (code {})",
            value.kind.repr as u32
        ));
    })
}

fn to_request(
    payload: &Payload,
    options: &RenderOptions,
) -> Result<vynx_qr_core::qr::RenderRequest, String> {
    use vynx_qr_core::qr::{QrStyle, RenderRequest};

    let style = QrStyle {
        module_style: core_module_style(options.module_style),
        foreground: options.foreground.clone(),
        background: options.background.clone(),
        quiet_zone: options.quiet_zone,
        logo_ratio: options.logo_ratio,
        logo: options.has_logo.then(|| vynx_qr_core::qr::LogoInput {
            name: options.logo.name.clone(),
            data: options.logo.data.clone(),
        }),
    };

    Ok(RenderRequest {
        payload: to_core_payload(payload)?,
        style,
        ec_level: core_ec_level(options.ec_level),
        size_px: options.size_px,
    })
}

/// Convert a core error into something the window can show.
///
/// `AppError` already renders as `[Code] message (detail)`, and every part of it
/// is written to be read by a person, so there is nothing to translate.
fn describe(error: vynx_qr_core::AppError) -> String {
    error.to_string()
}

// cxx represents a shared enum as a struct holding a `repr` byte, so it cannot be
// matched exhaustively from Rust. These helpers convert by comparison, treating
// anything unrecognised as the documented default, which is the safe direction
// for every one of them: an unknown variant becomes the conservative or the
// higher-error-correction choice rather than a silent surprise.

fn core_module_style(style: ModuleStyle) -> vynx_qr_core::qr::payload::ModuleStyle {
    if style == ModuleStyle::Rounded {
        vynx_qr_core::qr::payload::ModuleStyle::Rounded
    } else {
        vynx_qr_core::qr::payload::ModuleStyle::Square
    }
}

fn bridge_module_style(style: vynx_qr_core::qr::payload::ModuleStyle) -> ModuleStyle {
    if style == vynx_qr_core::qr::payload::ModuleStyle::Rounded {
        ModuleStyle::Rounded
    } else {
        ModuleStyle::Square
    }
}

fn core_ec_level(level: EcLevel) -> vynx_qr_core::qr::payload::EcLevel {
    use vynx_qr_core::qr::payload::EcLevel as Core;
    if level == EcLevel::L {
        Core::L
    } else if level == EcLevel::Q {
        Core::Q
    } else if level == EcLevel::M {
        Core::M
    } else {
        Core::H
    }
}

fn bridge_ec_level(level: vynx_qr_core::qr::payload::EcLevel) -> EcLevel {
    use vynx_qr_core::qr::payload::EcLevel as Core;
    match level {
        Core::L => EcLevel::L,
        Core::M => EcLevel::M,
        Core::Q => EcLevel::Q,
        Core::H => EcLevel::H,
    }
}

fn core_wifi_security(security: WifiSecurity) -> vynx_qr_core::qr::payload::WifiSecurity {
    if security == WifiSecurity::Wep {
        vynx_qr_core::qr::payload::WifiSecurity::Wep
    } else if security == WifiSecurity::None {
        vynx_qr_core::qr::payload::WifiSecurity::None
    } else {
        vynx_qr_core::qr::payload::WifiSecurity::Wpa
    }
}

fn bridge_verify_status(status: vynx_qr_core::qr::VerifyStatus) -> VerifyStatus {
    if status == vynx_qr_core::qr::VerifyStatus::Failed {
        VerifyStatus::Failed
    } else if status == vynx_qr_core::qr::VerifyStatus::Mismatch {
        VerifyStatus::Mismatch
    } else {
        VerifyStatus::Verified
    }
}

fn core_theme(theme: ThemeMode) -> vynx_qr_core::settings::ThemeMode {
    if theme == ThemeMode::Light {
        vynx_qr_core::settings::ThemeMode::Light
    } else if theme == ThemeMode::Dark {
        vynx_qr_core::settings::ThemeMode::Dark
    } else {
        vynx_qr_core::settings::ThemeMode::System
    }
}

fn bridge_theme(theme: vynx_qr_core::settings::ThemeMode) -> ThemeMode {
    if theme == vynx_qr_core::settings::ThemeMode::Light {
        ThemeMode::Light
    } else if theme == vynx_qr_core::settings::ThemeMode::Dark {
        ThemeMode::Dark
    } else {
        ThemeMode::System
    }
}

fn core_export_format(format: ExportFormat) -> vynx_qr_core::settings::ExportFormat {
    if format == ExportFormat::Svg {
        vynx_qr_core::settings::ExportFormat::Svg
    } else {
        vynx_qr_core::settings::ExportFormat::Png
    }
}

fn bridge_export_format(format: vynx_qr_core::settings::ExportFormat) -> ExportFormat {
    if format == vynx_qr_core::settings::ExportFormat::Svg {
        ExportFormat::Svg
    } else {
        ExportFormat::Png
    }
}

/// Convert an engine payload into the bridge representation.
///
/// The counterpart of [`to_core_payload`]. Every kind is matched explicitly, so
/// adding one to the engine is a compile error here rather than a payload that
/// silently encodes as something else.
fn from_core_payload(core: &vynx_qr_core::qr::payload::QrPayload) -> Payload {
    use vynx_qr_core::qr::payload::{QrPayload, WifiSecurity as CoreSecurity};

    let mut payload = Payload {
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
        sms: SmsPayload {
            number: String::new(),
            message: String::new(),
        },
        geo: GeoPayload {
            latitude: 0.0,
            longitude: 0.0,
            label: String::new(),
        },
    };

    match core {
        QrPayload::Text { text } => {
            payload.kind = PayloadType::Text;
            payload.text = text.clone();
        }
        QrPayload::Url { url } => {
            payload.kind = PayloadType::Url;
            payload.url = url.clone();
        }
        QrPayload::Wifi {
            ssid,
            password,
            security,
            hidden,
        } => {
            payload.kind = PayloadType::Wifi;
            payload.wifi = WifiPayload {
                ssid: ssid.clone(),
                password: password.clone(),
                security: match security {
                    CoreSecurity::Wpa => WifiSecurity::Wpa,
                    CoreSecurity::Wep => WifiSecurity::Wep,
                    CoreSecurity::None => WifiSecurity::None,
                },
                hidden: *hidden,
            };
        }
        QrPayload::VCard {
            first_name,
            last_name,
            organization,
            job_title,
            phone,
            email,
            website,
            address,
            note,
        } => {
            payload.kind = PayloadType::VCard;
            payload.vcard = VCardPayload {
                first_name: first_name.clone(),
                last_name: last_name.clone(),
                organization: organization.clone(),
                job_title: job_title.clone(),
                phone: phone.clone(),
                email: email.clone(),
                website: website.clone(),
                address: address.clone(),
                note: note.clone(),
            };
        }
        QrPayload::Email { to, subject, body } => {
            payload.kind = PayloadType::Email;
            payload.email = EmailPayload {
                to: to.clone(),
                subject: subject.clone(),
                body: body.clone(),
            };
        }
        QrPayload::Phone { number } => {
            payload.kind = PayloadType::Phone;
            payload.phone = number.clone();
        }
        QrPayload::Sms { number, message } => {
            payload.kind = PayloadType::Sms;
            payload.sms = SmsPayload {
                number: number.clone(),
                message: message.clone(),
            };
        }
        QrPayload::Geo {
            latitude,
            longitude,
            label,
        } => {
            payload.kind = PayloadType::Geo;
            payload.geo = GeoPayload {
                latitude: *latitude,
                longitude: *longitude,
                label: label.clone(),
            };
        }
    }
    payload
}

/// Turn free-form input into both the classification and the payload, so the
/// window never has to reconstruct either from the encoded string.
pub fn smart_payload(input: &str) -> Result<SmartPayload, String> {
    use vynx_qr_core::qr::payload::QrPayload;

    guard("Analysis", || {
        let Some(found) = vynx_qr_core::detect::analyze_input_checked(input).map_err(describe)?
        else {
            let blank = Analysis {
                detected: false,
                kind: ContentKind::Text,
                original: input.to_string(),
                encoded: String::from(""),
                normalization: String::from(""),
                notice: String::from(""),
                changes_input: false,
            };
            return Ok(SmartPayload {
                detected: false,
                analysis: blank,
                payload: from_core_payload(&QrPayload::Text {
                    text: String::new(),
                }),
            });
        };

        let analysis = Analysis {
            detected: true,
            kind: match found.kind {
                vynx_qr_core::detect::ContentKind::Url => ContentKind::Url,
                vynx_qr_core::detect::ContentKind::Email => ContentKind::Email,
                vynx_qr_core::detect::ContentKind::Phone => ContentKind::Phone,
                vynx_qr_core::detect::ContentKind::Text => ContentKind::Text,
            },
            original: found.input.clone(),
            encoded: found.encoded.clone(),
            changes_input: found.changes_the_input(),
            normalization: found.normalization.clone().unwrap_or_default(),
            notice: found.notice.clone().unwrap_or_default(),
        };

        Ok(SmartPayload {
            detected: true,
            analysis,
            payload: from_core_payload(&found.payload),
        })
    })
}

pub fn original_text_payload(input: &str) -> Payload {
    from_core_payload(&vynx_qr_core::detect::original_text_payload(input))
}

pub fn analyze(input: &str) -> Result<Analysis, String> {
    guard("Analysis", || {
        let Some(found) = vynx_qr_core::detect::analyze_input_checked(input).map_err(describe)?
        else {
            return Ok(Analysis {
                detected: false,
                kind: ContentKind::Text,
                original: input.to_string(),
                encoded: String::from(""),
                normalization: String::from(""),
                notice: String::from(""),
                changes_input: false,
            });
        };

        Ok(Analysis {
            detected: true,
            kind: match found.kind {
                vynx_qr_core::detect::ContentKind::Url => ContentKind::Url,
                vynx_qr_core::detect::ContentKind::Email => ContentKind::Email,
                vynx_qr_core::detect::ContentKind::Phone => ContentKind::Phone,
                vynx_qr_core::detect::ContentKind::Text => ContentKind::Text,
            },
            original: found.input.clone(),
            encoded: found.encoded.clone(),
            normalization: found.normalization.clone().unwrap_or_default(),
            notice: found.notice.clone().unwrap_or_default(),
            changes_input: found.changes_the_input(),
        })
    })
}

pub fn generate(payload: &Payload, options: &RenderOptions) -> Result<GenerateResult, String> {
    guard("Rendering", || {
        use vynx_qr_core::export::render_png;

        let request = to_request(payload, options)?;
        let result = render_png(&request).map_err(describe)?;

        Ok(GenerateResult {
            png: result.png,
            width: result.width,
            height: result.height,
            modules: result.modules,
            total_modules: result.total_modules,
            quiet_zone: result.quiet_zone,
            version: u32::from(result.version),
            ec_level: bridge_ec_level(result.ec_level),
            ec_adjusted: result.ec_adjusted,
            contrast: result.contrast,
            verification_status: bridge_verify_status(result.verification.status),
            has_reduced_verification: result.verification.reduced.is_some(),
            reduced_verification: result
                .verification
                .reduced
                .map(bridge_verify_status)
                // Unreachable while the flag is false, but a shared enum cannot be
                // "absent", so this has to name something. Verified is the safe
                // default: the window only reads it when the flag is true.
                .unwrap_or(VerifyStatus::Verified),
            decoded: result.verification.decoded.unwrap_or_default(),
            warnings: result
                .warnings
                .into_iter()
                .map(|warning| Warning {
                    code: warning.code,
                    message: warning.message,
                })
                .collect(),
            encoded: result.encoded,
        })
    })
}

pub fn generate_svg(
    payload: &Payload,
    options: &RenderOptions,
    size_px: u32,
) -> Result<String, String> {
    guard("SVG export", || {
        let request = to_request(payload, options)?;
        vynx_qr_core::export::render_svg(&request, size_px).map_err(describe)
    })
}

pub fn generate_bitmap(payload: &Payload, options: &RenderOptions) -> Result<Bitmap, String> {
    guard("Bitmap rendering", || {
        let request = to_request(payload, options)?;
        let canvas = vynx_qr_core::export::render_rgba(&request).map_err(describe)?;
        Ok(Bitmap {
            width: canvas.width,
            height: canvas.height,
            pixels: canvas.pixels,
        })
    })
}

pub fn is_encodable(payload: &Payload) -> bool {
    to_core_payload(payload)
        .map(|core| vynx_qr_core::export::is_encodable(&core))
        .unwrap_or(false)
}

/// The name of a payload kind, for the status line and for file naming.
///
/// This is deliberately the *kind*, not the content: a status chip reads "Wi-Fi"
/// whether the SSID happens to be filled in yet, and a suggested file name is
/// built from the kind before the user has typed anything.
pub fn payload_label(kind: PayloadType) -> String {
    if kind == PayloadType::Text {
        "Text".to_string()
    } else if kind == PayloadType::Url {
        "URL".to_string()
    } else if kind == PayloadType::Wifi {
        "Wi-Fi".to_string()
    } else if kind == PayloadType::VCard {
        "Contact".to_string()
    } else if kind == PayloadType::Email {
        "Email".to_string()
    } else if kind == PayloadType::Phone {
        "Phone".to_string()
    } else if kind == PayloadType::Sms {
        "SMS".to_string()
    } else if kind == PayloadType::Geo {
        "Location".to_string()
    } else {
        // Same reasoning as `to_core_payload`: an unrecognised kind is reported,
        // not guessed at. Guessing would label a payload type this build cannot
        // encode.
        format!("Unsupported (code {})", kind.repr as u32)
    }
}

pub fn load_logo(path: &str) -> Result<LogoAsset, String> {
    guard("Logo loading", || {
        let asset = vynx_qr_core::export::load_logo(path).map_err(describe)?;
        Ok(LogoAsset {
            name: asset.name,
            width: asset.width,
            height: asset.height,
        })
    })
}

pub fn system_info() -> SystemInfo {
    let info = vynx_qr_core::platform::system_info();
    SystemInfo {
        os_build: info.os_build,
        is_windows_11: info.is_windows_11,
        accent_color: info.accent_color.unwrap_or_default(),
        development: info.development,
    }
}

pub fn load_settings() -> Settings {
    from_settings(&vynx_qr_core::settings::load())
}

pub fn save_settings(settings: &Settings) -> Result<(), String> {
    guard("Saving settings", || {
        vynx_qr_core::settings::save(&to_settings(settings)).map_err(describe)
    })
}

pub fn default_style() -> RenderOptions {
    let style = vynx_qr_core::QrStyle::default();
    RenderOptions {
        size_px: 1024,
        ec_level: EcLevel::M,
        module_style: ModuleStyle::Square,
        foreground: style.foreground.clone(),
        background: style.background.clone(),
        quiet_zone: style.quiet_zone,
        logo_ratio: style.logo_ratio,
        has_logo: false,
        logo: Logo {
            name: String::new(),
            data: Vec::new(),
        },
    }
}

fn from_settings(settings: &vynx_qr_core::settings::Settings) -> Settings {
    Settings {
        theme: bridge_theme(settings.theme),
        use_windows_accent: settings.use_windows_accent,
        clipboard_check: settings.clipboard_check,
        auto_paste: settings.auto_paste,
        default_format: bridge_export_format(settings.default_format),
        default_size: settings.default_size,
        default_error_correction: bridge_ec_level(settings.default_error_correction),
        default_module_style: bridge_module_style(settings.default_module_style),
    }
}

fn to_settings(value: &Settings) -> vynx_qr_core::settings::Settings {
    vynx_qr_core::settings::Settings {
        theme: core_theme(value.theme),
        use_windows_accent: value.use_windows_accent,
        clipboard_check: value.clipboard_check,
        auto_paste: value.auto_paste,
        default_format: core_export_format(value.default_format),
        default_size: value.default_size,
        default_error_correction: core_ec_level(value.default_error_correction),
        default_module_style: if value.default_module_style == ModuleStyle::Rounded {
            vynx_qr_core::qr::payload::ModuleStyle::Rounded
        } else {
            vynx_qr_core::qr::payload::ModuleStyle::Square
        },
    }
}
