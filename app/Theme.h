#pragma once

#include <QColor>
#include <QString>

/// Design tokens.
///
/// Every measurement and colour in the window comes from here. No widget hard
/// codes a pixel value or a hex string, so the light and dark themes stay in step
/// and a spacing change is one edit rather than a sweep through the code.

namespace Tokens {

// Spacing, in device independent pixels.
inline constexpr int SpaceSmall = 8;
inline constexpr int SpaceMedium = 12;
inline constexpr int SpaceLarge = 16;
inline constexpr int SpaceXLarge = 24;

// Corner radii.
inline constexpr int RadiusSmall = 6;
inline constexpr int RadiusMedium = 8;
inline constexpr int RadiusLarge = 12;

// The window is one screen tall at most; this keeps it sensible on a laptop.
inline constexpr int WindowWidth = 960;
inline constexpr int WindowHeight = 680;
inline constexpr int WindowMinimumWidth = 720;
inline constexpr int WindowMinimumHeight = 560;

// The preview never exceeds this, so a 2048 pixel export does not become a
// 2048 pixel widget.
inline constexpr int PreviewMaximum = 420;

} // namespace Tokens

/// One resolved palette, produced from a theme name and an optional accent.
struct Palette {
  QColor window;
  QColor surface;
  QColor surfaceSunken;
  QColor border;
  QColor textPrimary;
  QColor textSecondary;
  QColor textTertiary;
  QColor accent;
  QColor accentText;
  QColor success;
  QColor warning;
  QColor danger;

  bool isDark() const { return window.lightness() < 128; }
};

/// The brand colour, used when Windows reports no accent of its own.
QColor vynxBlue();

/// Build a palette for the given theme, optionally following the Windows accent.
Palette makePalette(const QString &theme, const QColor &accent);

/// The stylesheet for a palette. Kept beside the palette so the two cannot drift.
QString styleSheetFor(const Palette &palette);

/// Read the system font, preferring Segoe UI Variable when the host has it.
QString systemFontFamily();