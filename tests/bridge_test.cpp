// Proves the C++ side can drive the Rust engine end to end.
//
// If this links, runs and passes, then three things are true at once: the
// generated header matches the bridge, the static library links into a C++
// program, and the engine answers correctly when called from C++.
//
// cxx reports a fallible bridge function as a plain C++ function returning the
// value and throwing `rust::Error` on failure, so failure is handled with
// try/catch here rather than a checked return.

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

#include <rust/cxx.h>

#include "vynx-qr-bridge/src/lib.rs.h"

namespace {

int failures = 0;

void check(bool condition, const std::string &what) {
  if (!condition) {
    std::cerr << "  FAIL: " << what << '\n';
    failures += 1;
  } else {
    std::cout << "  ok: " << what << '\n';
  }
}

std::string text(const rust::String &value) {
  return std::string(value.data(), value.size());
}

/// A detached Rust string, for the arguments the bridge takes by value.
rust::String own(const std::string &value) { return rust::String(value); }

vynx::Payload textPayload(const std::string &value) {
  vynx::Payload payload;
  payload.kind = vynx::PayloadType::Text;
  payload.text = own(value);
  payload.url = own("");
  payload.phone = own("");
  return payload;
}

void testDetection() {
  std::cout << "detection\n";
  const auto analysis = vynx::analyze(rust::Str("github.com"));
  check(analysis.detected, "a bare domain is detected");
  check(analysis.kind == vynx::ContentKind::Url, "github.com is a URL");
  check(text(analysis.encoded) == "https://github.com", "the scheme is added");
  check(!text(analysis.normalization).empty(), "the added scheme is announced");

  const auto blank = vynx::analyze(rust::Str("   "));
  check(!blank.detected, "blank input detects nothing");
}

void testRendering() {
  std::cout << "rendering\n";
  const auto result = vynx::generate(textPayload("VYNX QR"), vynx::default_style());
  check(result.png.size() > 8, "PNG bytes come back");
  check(result.png[0] == 0x89 && result.png[1] == 'P' && result.png[2] == 'N' &&
            result.png[3] == 'G',
        "the bytes really are a PNG");
  check(result.width == result.height, "the code is square");
  check(result.verification_status == vynx::VerifyStatus::Verified,
        "the code decodes back to the payload");
  check(text(result.encoded) == "VYNX QR", "the payload survived the round trip");
}

void testVectorExport() {
  std::cout << "vector export\n";
  const auto document = vynx::generate_svg(textPayload("VYNX QR"), vynx::default_style(), 512);
  const std::string body = text(document);
  check(body.find("<svg") != std::string::npos, "it is an SVG");
  check(body.find("data:image") == std::string::npos,
        "no raster is embedded without a logo");
}

void testUnicode() {
  std::cout << "unicode\n";
  for (const char *value : {"Привіт, Україно \U0001F1FA\U0001F1E6", "こんにちは", "🙂"}) {
    const auto result = vynx::generate(textPayload(value), vynx::default_style());
    check(text(result.encoded) == value, std::string("unchanged: ") + value);
    check(result.verification_status == vynx::VerifyStatus::Verified,
          std::string("scans back: ") + value);
  }
}

void testWifiIsNotTrimmed() {
  std::cout << "wifi credentials\n";
  vynx::Payload payload = textPayload("");
  payload.kind = vynx::PayloadType::Wifi;
  payload.wifi.ssid = own(" VYNX Home ");
  payload.wifi.password = own(" password with spaces ");
  payload.wifi.security = vynx::WifiSecurity::Wpa;

  const auto result = vynx::generate(payload, vynx::default_style());
  check(text(result.encoded) == "WIFI:T:WPA;S: VYNX Home ;P: password with spaces ;;",
        "surrounding spaces survive");
  check(result.verification_status == vynx::VerifyStatus::Verified,
        "the Wi-Fi code scans back");
}

/// The whole smart input path, from typed text to the decoded symbol.
///
/// These are the acceptance cases for the universal input field: each one goes
/// C++ -> CXX -> Rust -> QR -> decode, because the bugs worth catching here are
/// all in the seams rather than in any single layer.
void testSmartInput() {
  std::cout << "smart input\n";

  const auto render = [](const std::string &input) {
    const auto smart = vynx::smart_payload(rust::Str(input));
    const auto result = vynx::generate(smart.payload, vynx::default_style());
    return std::make_pair(text(result.encoded), result.verification_status);
  };

  {
    const auto smart = vynx::smart_payload(rust::Str("github.com"));
    check(smart.detected, "github.com is detected");
    check(smart.analysis.kind == vynx::ContentKind::Url, "github.com is a URL");
    check(!text(smart.analysis.normalization).empty(), "the added scheme is announced");
    check(smart.analysis.changes_input, "the payload differs from what was typed");

    const auto [encoded, status] = render("github.com");
    check(encoded == "https://github.com", "the QR holds the normalised URL");
    check(status == vynx::VerifyStatus::Verified, "and it scans back");
  }

  {
    // The important one: the engine already added "mailto:", so feeding that
    // string back into the email encoder would produce "mailto:mailto:...".
    const auto smart = vynx::smart_payload(rust::Str("hello@example.com"));
    check(smart.analysis.kind == vynx::ContentKind::Email, "hello@example.com is an email");
    check(std::string(text(smart.analysis.encoded).c_str()) == "mailto:hello@example.com",
          "the encoded form carries the scheme once");

    const auto [encoded, status] = render("hello@example.com");
    check(encoded == "mailto:hello@example.com", "the QR holds exactly one mailto:");
    check(encoded.find("mailto:mailto:") == std::string::npos, "the scheme is not doubled");
    check(status == vynx::VerifyStatus::Verified, "and it scans back");
  }

  {
    const auto smart = vynx::smart_payload(rust::Str("+380 99 123 4567"));
    check(smart.analysis.kind == vynx::ContentKind::Phone, "the number is a phone number");

    const auto [encoded, status] = render("+380 99 123 4567");
    check(encoded == "tel:+380991234567", "the QR holds exactly one tel:");
    check(encoded.find("tel:tel:") == std::string::npos, "the scheme is not doubled");
    check(status == vynx::VerifyStatus::Verified, "and it scans back");
  }

  {
    const auto smart = vynx::smart_payload(rust::Str("Привіт, Україно 🇺🇦"));
    check(smart.analysis.kind == vynx::ContentKind::Text, "prose is plain text");
    check(!smart.analysis.changes_input, "nothing was changed, so nothing is announced");

    const auto [encoded, status] = render("Привіт, Україно 🇺🇦");
    check(encoded == "Привіт, Україно 🇺🇦", "the QR holds the text exactly");
    check(status == vynx::VerifyStatus::Verified, "and it scans back");
  }

  {
    // An input that needs no normalisation must not offer the escape hatch.
    const auto smart = vynx::smart_payload(rust::Str("https://github.com"));
    check(!smart.analysis.changes_input, "a complete URL is left alone");
    check(text(smart.analysis.normalization).empty(), "and nothing is announced");
  }
}

/// "Keep exactly what I typed", which must encode the raw text rather than the
/// normalised URL.
void testOriginalText() {
  std::cout << "original text\n";

  const auto original = vynx::original_text_payload(rust::Str("github.com"));
  check(original.kind == vynx::PayloadType::Text, "the alternative is a text payload");
  check(text(original.text) == "github.com", "holding exactly what was typed");

  const auto result = vynx::generate(original, vynx::default_style());
  check(text(result.encoded) == "github.com", "the QR holds the bare text");
  check(result.verification_status == vynx::VerifyStatus::Verified, "and it scans back");

  const auto blank = vynx::smart_payload(rust::Str("   "));
  check(!blank.detected, "blank input is not detected");
}

void testRefusals() {

  bool refused = false;
  try {
    vynx::Payload payload = textPayload("");
    payload.kind = vynx::PayloadType::Wifi;
    const auto unused = vynx::generate(payload, vynx::default_style());
    (void)unused;
  } catch (const rust::Error &error) {
    refused = true;
    check(std::string(error.what()).find("network name") != std::string::npos,
          "an empty SSID is refused with a useful message");
  }
  check(refused, "an empty SSID throws rather than encoding nothing");

  bool refusedUnknown = false;
  try {
    vynx::Payload unknown = textPayload("hello");
    unknown.kind = vynx::PayloadType{200};
    const auto unused = vynx::generate(unknown, vynx::default_style());
    (void)unused;
  } catch (const rust::Error &error) {
    refusedUnknown = true;
    check(std::string(error.what()).find("Unsupported payload type") != std::string::npos,
          "an unknown payload kind is named in the error");
  }
  check(refusedUnknown, "an unknown payload kind throws rather than becoming Geo");
}

} // namespace

int main() {
  std::cout << "VYNX QR bridge tests\n";
  try {
    testDetection();
    testRendering();
    testVectorExport();
    testUnicode();
    testWifiIsNotTrimmed();
    testSmartInput();
    testOriginalText();
    testRefusals();
  } catch (const rust::Error &error) {
    std::cerr << "unexpected engine failure: " << error.what() << '\n';
    return EXIT_FAILURE;
  }

  if (failures == 0) {
    std::cout << "all checks passed\n";
    return EXIT_SUCCESS;
  }
  std::cerr << failures << " check(s) failed\n";
  return EXIT_FAILURE;
}