# Modern UI & 1.2.0 Feature Update Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Transform VYNX QR into a modern, card-based interface matching the new design mockups, adding the "Dots" QR module style in Rust, color pickers, text captions under QR codes, category pills with a "More..." menu, and a sleek top bar with theme toggle.

**Architecture:** 
1. The Rust core (`vynx_qr_core`) and CXX bridge (`vynx-qr-bridge`) are extended with a third `ModuleStyle::Dots` rendering mode (raster and SVG).
2. The Qt presentation layer (`app/Theme.*`, `app/MainWindow.*`, `app/widgets/*`) is restructured into a clean two-column card layout with a top navigation bar, pill-shaped payload selectors, visual style cards, color swatch pickers, and caption composition.
3. Multilingual localization (`I18n.*`) is updated across Ukrainian, English, and Russian for all new controls and copy.

**Tech Stack:** C++17, Qt 6.8 Widgets, Rust (2021 edition, `qrcode`, `rqrr`, `image`), CMake, CXX bridge.

**Spec:** User reference mockup (`media_1795538837162_b3575401.jpg`):
- Top bar: Brand icon + "QR Generator", Dark/Light mode toggle, Scanner, About button.
- Left column card:
  - Header: Icon + "Create QR code" + subtitle.
  - Payload selector pills: Link, Text, Email, Phone, Wi-Fi, and "More..." dropdown.
  - Dynamic input field with leading icon and clear (✕) button.
  - Settings section: 3 style cards (Classic / Rounded / Dots), QR color and background color pickers, checkbox "Add text under QR-code" with caption input.
  - Action buttons: "Generate QR code" (primary purple) and "Clear" (secondary).
- Right column card:
  - White rounded QR preview container.
  - Action buttons: "Download (PNG)" (primary purple) and "Copy" (secondary).

## Global Constraints

- Must compile cleanly on Windows (MSVC 2022) and Linux (GCC/Clang).
- Qt LGPL-3.0 compliance: dynamic linking, clean widget hierarchy, no proprietary closed dependencies.
- Zero telemetry, 100% offline and local operation.
- Complete 3-language localization support: Ukrainian, English, Russian.
- All existing payload kinds (2FA, Crypto, SBP, Geo, Event, SMS, Contact) and features (Screen Snipper, Drag & Drop QR decoder, Auto-Updater) must remain fully functional.

## Review Focus

1. **Dots rendering quality & scannability**: Dot modules must be centered within cells, with finder patterns remaining crisp and 100% decodable by `rqrr`.
2. **Caption layout & high-DPI scaling**: Text rendered under the QR code must have correct padding, centered alignment, legible font scaling, and matching foreground/background colors without clipping.
3. **Dynamic retranslation**: Switching language between EN, UK, and RU in Settings must immediately retranslate all new headers, pills, style cards, and buttons without restarting.
4. **Theme switching**: Toggling dark/light mode in the top bar must instantly update cards, pill states, input backgrounds, borders, and QR preview container.
5. **Memory & Performance**: Live typing debouncing must prevent lag; raster buffers must not leak.

---

### Task 1: Rust Core & Bridge — Add `Dots` ModuleStyle

**Files:**
- Modify: `core/src/qr/payload.rs:86-98`
- Modify: `core/src/qr/render.rs:175-195`
- Modify: `core/src/qr/svg.rs:50-75`
- Modify: `bridge/src/lib.rs:49-60, 569-585`
- Test: `core/tests/round_trip.rs`
- Test: `tests/MainWindow_test.cpp`

**Interfaces:**
- Consumes: `QrMatrix`, `ResolvedStyle`, `Canvas`
- Produces: `vynx::ModuleStyle::Dots` in C++ and `vynx_qr_core::qr::payload::ModuleStyle::Dots` in Rust

- [ ] **Step 1: Write failing Rust test for `ModuleStyle::Dots`**

In `core/tests/round_trip.rs`:
```rust
#[test]
fn dots_style_rasterizes_and_scans() {
    let request = RenderRequest {
        payload: QrPayload::Text("https://vynx.org".into()),
        style: StyleRequest {
            foreground: "#000000".into(),
            background: "#ffffff".into(),
            module_style: vynx_qr_core::qr::payload::ModuleStyle::Dots,
            quiet_zone: 4,
            logo: None,
        },
        ec_level: None,
        size_px: 512,
    };
    let result = vynx_qr_core::export::render_png(&request).expect("render should succeed");
    assert!(result.verified, "Dots style QR code must be verified and readable by rqrr");
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cargo test --package vynx-qr-core dots_style_rasterizes_and_scans`
Expected: FAIL due to missing variant `ModuleStyle::Dots`.

- [ ] **Step 3: Implement `ModuleStyle::Dots` in `core` and `bridge`**

1. In `core/src/qr/payload.rs`: Add `Dots` to `enum ModuleStyle { Square, Rounded, Dots }`.
2. In `core/src/qr/render.rs`:
   Implement `fill_circle(canvas, cx, cy, radius, color)`:
   For regular modules when `style.module_style == ModuleStyle::Dots`, draw a centered circle with radius `(cell as f32 * 0.45)` (or `cell / 2`).
   In `draw_finders`: keep position detection patterns scannable (outer ring, inner gap, center dot).
3. In `core/src/qr/svg.rs`: Render `<circle cx="..." cy="..." r="..." fill="..." />` for dots modules.
4. In `bridge/src/lib.rs`: Add `Dots` to `enum ModuleStyle { Square, Rounded, Dots }` in `#[cxx::bridge]` and update bridge conversion functions.

- [ ] **Step 4: Run tests to verify they pass**

Run: `cargo test --all`
Expected: PASS for all core and bridge tests.

- [ ] **Step 5: Commit**

```bash
git add core/ bridge/
git commit -m "feat(core): add Dots module style to QR engine and bridge"
```

---

### Task 2: Design Tokens & Palette Update for Modern Card UI

**Files:**
- Modify: `app/Theme.h`
- Modify: `app/Theme.cpp`
- Test: `tests/MainWindow_test.cpp`

**Interfaces:**
- Consumes: `Tokens`, `Palette`
- Produces: Updated palettes with modern vibrant purple brand color (`#6366F1`), card styles (`QFrame[role="card"]`), pill button styles (`QPushButton[role="pill"]`), style selector cards (`QAbstractButton[role="styleCard"]`), and clear-button styles.

- [ ] **Step 1: Write test for updated Theme tokens and palette**

In `tests/MainWindow_test.cpp`:
```cpp
void themeTokensAndBrandPaletteValid() {
    auto palette = makePalette(QStringLiteral("light"), QColor());
    QCOMPARE(vynxBlue(), QColor("#6366F1")); // Modern brand purple
    QVERIFY(palette.surface.isValid());
    QVERIFY(palette.window.isValid());
    QString qss = styleSheetFor(palette);
    QVERIFY(qss.contains("QFrame[role=\"card\"]"));
    QVERIFY(qss.contains("QPushButton[role=\"pill\"]"));
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `ctest --preset release -R MainWindow_test`
Expected: FAIL on brand color and missing card/pill QSS selectors.

- [ ] **Step 3: Update `app/Theme.h` and `app/Theme.cpp`**

1. Change `vynxBlue()` to `#6366F1` (or `#6C5CE7` matching mockup).
2. Refine `LightWindow` to `#F4F5F7` / `#F8F9FA`, `LightSurface` to `#FFFFFF`, `LightBorder` to `#E5E7EB`.
3. Add QSS rules for:
   - `QFrame[role="card"]`: rounded background, subtle border, padding.
   - `QPushButton[role="pill"]`: rounded pill button (`border-radius: 16px`), distinct active/checked state (purple background with white text) vs inactive state.
   - `QAbstractButton[role="styleCard"]`: visual card for selecting QR styles with icon + label, purple border when checked.
   - `QLineEdit`: clean padded input field with rounded border (`border-radius: 8px`).
   - `QToolButton[role="topBarButton"]`: borderless icon buttons for theme toggle, scan, about.

- [ ] **Step 4: Run test to verify it passes**

Run: `ctest --preset release -R MainWindow_test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add app/Theme.h app/Theme.cpp tests/MainWindow_test.cpp
git commit -m "feat(theme): update design tokens, card styles, and pill button theme"
```

---

### Task 3: Build Modern Top Bar and Two-Column Card Layout

**Files:**
- Create: `app/widgets/StyleSelectorWidget.h`, `app/widgets/StyleSelectorWidget.cpp`
- Modify: `app/MainWindow.h`
- Modify: `app/MainWindow.cpp`
- Modify: `app/i18n/I18n.h`, `app/i18n/I18n.cpp`
- Test: `tests/MainWindow_test.cpp`

**Interfaces:**
- Consumes: `StyleSelectorWidget`, `SpecialQrForms`, `vynx::ModuleStyle`
- Produces: Two-column card UI in `MainWindow` with top navigation bar, category pills (Link, Text, Email, Phone, Wi-Fi, More...), style selector cards, color pickers, and action buttons.

- [ ] **Step 1: Write test for StyleSelectorWidget and top-bar actions**

In `tests/MainWindow_test.cpp`:
```cpp
void styleSelectorSwitchesModes() {
    MainWindow window;
    // Verify default style is Classic (Square)
    // Select Dots -> verify currentOptions().module_style == vynx::ModuleStyle::Dots
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `ctest --preset release -R MainWindow_test`
Expected: FAIL due to missing `StyleSelectorWidget`.

- [ ] **Step 3: Implement `StyleSelectorWidget` and integrate in `MainWindow`**

1. Create `StyleSelectorWidget`:
   - Contains a button group with 3 cards: `Classic` (Square), `Rounded`, `Dots`.
   - Each card displays an icon representing the style and localized title.
   - Emits `styleChanged(vynx::ModuleStyle style)`.
2. In `MainWindow`:
   - Add Top Bar widget:
     - Logo icon + title "QR Generator" / "VYNX QR".
     - Spacer.
     - Dark/Light mode toggle button (switches between dark and light themes instantly).
     - QR Scanner button (triggers screen snipper / file scan).
     - Info button (opens About dialog).
   - Left Column Card (Composer):
     - Header: Icon + "Створити QR-код" / "Create QR code" + subtitle.
     - Category pills: URL, Text, Email, Phone, Wi-Fi, and "More..." dropdown menu (SMS, Geo, Event, 2FA, Crypto, SBP, Contact).
     - Smart input line with leading icon (🔗 / T) and clear button (✕).
     - Settings block:
       - Style code: `StyleSelectorWidget`.
       - Color code & Background color: clickable color buttons with color preview swatches.
     - Bottom buttons: "Generate QR code" (primary) and "Clear" (secondary).
3. Connect all signals/slots so switching category pills updates the input view cleanly.

- [ ] **Step 4: Run test to verify it passes**

Run: `ctest --preset release -R MainWindow_test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add app/widgets/StyleSelectorWidget.* app/MainWindow.* app/i18n/*
git commit -m "feat(ui): implement modern top bar, category pills, and card-based layout"
```

---

### Task 4: Text Under QR Code & QR Preview Card

**Files:**
- Modify: `app/widgets/QrPreview.h`
- Modify: `app/widgets/QrPreview.cpp`
- Modify: `app/MainWindow.h`
- Modify: `app/MainWindow.cpp`
- Test: `tests/MainWindow_test.cpp`

**Interfaces:**
- Consumes: `captionText`, `QImage`, `QPainter`
- Produces: Preview display with optional caption, and caption inclusion in clipboard copy and PNG/SVG export.

- [ ] **Step 1: Write test for caption composition under QR code**

In `tests/MainWindow_test.cpp`:
```cpp
void captionUnderQrCodeExportedAndCopied() {
    MainWindow window;
    // Set text "github.com"
    // Enable caption checkbox and set caption "Scan me"
    // Verify export/copy image height includes caption banner
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `ctest --preset release -R MainWindow_test`
Expected: FAIL.

- [ ] **Step 3: Implement caption support in `QrPreview` and `MainWindow`**

1. In `MainWindow`:
   - Add checkbox "Додати текст під QR-кодом" / "Add text under QR code" and `QLineEdit` for caption text in the Settings section.
   - When checkbox is toggled, reveal the caption input field and trigger `refresh()`.
2. In `QrPreview`:
   - Add caption label under the QR image container in `QrPreview`.
   - Update `showResult`: if caption text is set, display it below the code with proper font styling and color matching the QR code foreground.
3. In `MainWindow::copyToClipboard` and `exportToFile`:
   - If caption is enabled and not empty, use `QPainter` to composite the rendered QR code with the centered text caption below it (matching background and foreground colors) before copying or saving PNG.
4. Right Column Card:
   - Wrap `QrPreview` in a modern white rounded card with soft padding.
   - Bottom action buttons: "Завантажити (PNG)" / "Download (PNG)" primary purple button (with drop-down option for SVG) and "Копіювати" / "Copy" button.

- [ ] **Step 4: Run test to verify it passes**

Run: `ctest --preset release -R MainWindow_test`
Expected: PASS.

- [ ] **Step 5: Commit**

```bash
git add app/widgets/QrPreview.* app/MainWindow.* tests/MainWindow_test.cpp
git commit -m "feat(preview): add caption under QR code and update preview card controls"
```

---

### Task 5: Localization (UK, EN, RU), Version Bump to 1.2.0 & Verification

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `installer/vynx_qr.nsi`
- Modify: `app/i18n/I18n.h`, `app/i18n/I18n.cpp`
- Modify: `docs/release-notes.md`
- Test: All CMake and CTest suites

**Interfaces:**
- Consumes: Complete project sources
- Produces: Version 1.2.0 build with full Ukrainian, English, and Russian translations for all new elements.

- [ ] **Step 1: Write test verifying complete translations for all new keys**

In `tests/MainWindow_test.cpp`:
```cpp
void testAllNewUiStringsTranslatedInAllLanguages() {
    for (const auto &lang : {I18n::Language::English, I18n::Language::Russian, I18n::Language::Ukrainian}) {
        I18n::instance().setLanguage(lang);
        QVERIFY(!I18n::tr("create_qr_code").isEmpty());
        QVERIFY(!I18n::tr("style_classic").isEmpty());
        QVERIFY(!I18n::tr("style_rounded").isEmpty());
        QVERIFY(!I18n::tr("style_dots").isEmpty());
        QVERIFY(!I18n::tr("add_text_under_qr").isEmpty());
        QVERIFY(!I18n::tr("download_png").isEmpty());
    }
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `ctest --preset release -R MainWindow_test`
Expected: FAIL due to missing keys in `I18n`.

- [ ] **Step 3: Update `I18n` string tables and bump version to 1.2.0**

1. In `app/i18n/I18n.cpp`: Add complete string tables for UK, EN, RU for all new text:
   - UK: "Створити QR-код", "Перетворіть посилання, текст, email або номер телефону на QR-код", "Посилання", "Текст", "Email", "Телефон", "Wi-Fi", "Ще", "Стиль коду", "Класичний", "Округлий", "Точки", "Колір коду", "Колір фону", "Додати текст під QR-кодом", "Згенерувати QR-код", "Очистити", "Завантажити (PNG)", "Копіювати", etc.
   - EN and RU equivalents.
2. In `CMakeLists.txt`: bump version to `1.2.0`.
3. In `installer/vynx_qr.nsi`: bump version to `1.2.0`.
4. In `docs/release-notes.md`: add `VYNX QR 1.2.0` release notes.

- [ ] **Step 4: Run all tests and build verification**

Run: `cmake --build --preset release && ctest --preset release --output-on-failure`
Expected: 100% PASS for all tests.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt installer/vynx_qr.nsi app/i18n/ docs/release-notes.md tests/
git commit -m "chore: bump version to 1.2.0 and complete 3-language translations"
```
