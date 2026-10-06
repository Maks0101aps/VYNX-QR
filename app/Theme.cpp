#include "Theme.h"
#include "platform/PlatformIntegration.h"

#include <QApplication>
#include <QFontDatabase>
#include <QPalette>
#include <QStyleHints>

namespace {

/// Fluent inspired greys. Not pure white in light mode: a flat white window next
/// to a white browser reads as unfinished, so the canvas sits slightly off it.
inline const QColor LightWindow("#F3F3F3");
inline const QColor LightSurface("#FFFFFF");
inline const QColor LightSunken("#FAFAFA");
inline const QColor LightBorder("#DEDEDE");
inline const QColor LightPrimary("#1A1A1A");
inline const QColor LightSecondary("#5C5C5C");
inline const QColor LightTertiary("#8A8A8A");

inline const QColor DarkWindow("#202020");
inline const QColor DarkSurface("#2B2B2B");
inline const QColor DarkSunken("#1A1A1A");
inline const QColor DarkBorder("#3D3D3D");
inline const QColor DarkPrimary("#F2F2F2");
inline const QColor DarkSecondary("#B8B8B8");
inline const QColor DarkTertiary("#8C8C8C");

inline const QColor Success("#0F7B0F");
inline const QColor Warning("#9D5D00");
inline const QColor Danger("#C42B1C");

inline const QColor DarkSuccess("#5BD65B");
inline const QColor DarkWarning("#F2B441");
inline const QColor DarkDanger("#FF8A80");

/// Dark mode needs a readable foreground on a light accent and vice versa.
QColor readableOn(const QColor &background) {
  return background.lightness() > 140 ? QColor("#101010") : QColor("#FFFFFF");
}

} // namespace

QColor vynxBlue() { return QColor("#2F6FEB"); }

Palette makePalette(const QString &theme, const QColor &accent) {
  const bool systemDark =
      PlatformIntegration::systemPrefersDarkMode();
  const bool dark = theme == QStringLiteral("dark") || (theme == QStringLiteral("system") && systemDark);

  Palette palette;
  if (dark) {
    palette.window = DarkWindow;
    palette.surface = DarkSurface;
    palette.surfaceSunken = DarkSunken;
    palette.border = DarkBorder;
    palette.textPrimary = DarkPrimary;
    palette.textSecondary = DarkSecondary;
    palette.textTertiary = DarkTertiary;
    palette.success = DarkSuccess;
    palette.warning = DarkWarning;
    palette.danger = DarkDanger;
  } else {
    palette.window = LightWindow;
    palette.surface = LightSurface;
    palette.surfaceSunken = LightSunken;
    palette.border = LightBorder;
    palette.textPrimary = LightPrimary;
    palette.textSecondary = LightSecondary;
    palette.textTertiary = LightTertiary;
    palette.success = Success;
    palette.warning = Warning;
    palette.danger = Danger;
  }

  // An accent is a choice the user already made in the system, so it is used as
  // given. Only the text on top of it is adjusted for legibility.
  palette.accent = accent.isValid() ? accent : vynxBlue();
  palette.accentText = readableOn(palette.accent);
  return palette;
}

QString styleSheetFor(const Palette &palette) {
  const QString accent = palette.accent.name();
  const QString accentText = palette.accentText.name();

  return QStringLiteral(R"(
QWidget {
  font-family: "%1";
  font-size: 13px;
  color: %2;
}
QMainWindow, QDialog { background: %3; }

QLabel[role="heading"] {
  font-size: 20px;
  font-weight: 600;
  color: %2;
}
QLabel[role="caption"] { color: %4; font-size: 12px; }
QLabel[role="verificationGood"] { color: %5; font-weight: 600; }
QLabel[role="verificationBad"]  { color: %6; font-weight: 600; }

QLineEdit, QTextEdit, QPlainTextEdit, QComboBox, QSpinBox {
  background: %7;
  border: 1px solid %8;
  border-radius: %9px;
  padding: 8px 10px;
  selection-background-color: %10;
  selection-color: %11;
}
QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus, QComboBox:focus, QSpinBox:focus {
  border: 1px solid %10;
}
QLineEdit[invalid="true"] { border: 1px solid %6; }

QComboBox::drop-down { border: none; width: 24px; }

QPushButton {
  background: %7;
  border: 1px solid %8;
  border-radius: %9px;
  padding: 8px 16px;
  color: %2;
}
QPushButton:hover:enabled { background: %12; }
QPushButton:pressed:enabled { background: %8; }
QPushButton:disabled { color: %4; }

QPushButton[accent="true"] {
  background: %10;
  border: 1px solid %10;
  color: %11;
  font-weight: 600;
}
QPushButton[accent="true"]:hover:enabled { background: %13; }
QPushButton[accent="true"]:disabled { background: %8; color: %4; }

QPushButton[flat="true"] { background: transparent; border: none; }

QCheckBox, QRadioButton { spacing: 8px; }
QCheckBox::indicator, QRadioButton::indicator { width: 16px; height: 16px; }

QScrollArea { border: none; background: transparent; }
QScrollBar:vertical { width: 10px; background: transparent; margin: 0; }
QScrollBar::handle:vertical { background: %8; border-radius: 5px; min-height: 24px; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; }

QToolTip {
  background: %7;
  border: 1px solid %8;
  color: %2;
  padding: 4px 6px;
}
)")
      .arg(systemFontFamily(),
           palette.textPrimary.name(),
           palette.window.name(),
           palette.textSecondary.name(),
           palette.success.name(),
           palette.danger.name(),
           palette.surface.name(),
           palette.border.name())
      .arg(Tokens::RadiusMedium)
      .arg(accent)
      .arg(accentText)
      // Hover and pressed accent are derived rather than stored, so a custom
      // system accent still gets a complete set of interaction states.
      .arg(palette.accent.lighter(112).name())
      .arg(palette.accent.darker(112).name());
}

QString systemFontFamily() { return PlatformIntegration::systemFontFamily(); }
