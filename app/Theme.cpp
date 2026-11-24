#include "Theme.h"
#include "platform/PlatformIntegration.h"

#include <QApplication>
#include <QFontDatabase>
#include <QPalette>
#include <QStyleHints>

namespace {

/// Fluent inspired greys and modern card palette.
inline const QColor LightWindow("#F4F5F7");
inline const QColor LightSurface("#FFFFFF");
inline const QColor LightSunken("#F9FAFB");
inline const QColor LightBorder("#E5E7EB");
inline const QColor LightPrimary("#111827");
inline const QColor LightSecondary("#6B7280");
inline const QColor LightTertiary("#9CA3AF");

inline const QColor DarkWindow("#181824");
inline const QColor DarkSurface("#242436");
inline const QColor DarkSunken("#1E1E2E");
inline const QColor DarkBorder("#38384F");
inline const QColor DarkPrimary("#F3F4F6");
inline const QColor DarkSecondary("#9CA3AF");
inline const QColor DarkTertiary("#6B7280");

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

QColor vynxBlue() { return QColor("#6366F1"); }

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
  const QString hoverAccent = palette.accent.lighter(112).name();
  const QString pressedAccent = palette.accent.darker(112).name();

  QString qss = QStringLiteral(R"(
QWidget {
  font-family: "{{FONT}}";
  font-size: 13px;
  color: {{TEXT_PRIMARY}};
}
QMainWindow, QDialog { background: {{WINDOW}}; }

QFrame[role="card"] {
  background: {{SURFACE}};
  border: 1px solid {{BORDER}};
  border-radius: {{RADIUS_CARD}}px;
  padding: 16px;
}

QLabel[role="heading"] {
  font-size: 20px;
  font-weight: 600;
  color: {{TEXT_PRIMARY}};
}
QLabel[role="subheading"] { font-size: 13px; color: {{TEXT_SECONDARY}}; }
QLabel[role="caption"] { color: {{TEXT_SECONDARY}}; font-size: 12px; }
QLabel[role="verificationGood"] { color: {{SUCCESS}}; font-weight: 600; }
QLabel[role="verificationBad"]  { color: {{DANGER}}; font-weight: 600; }

QLineEdit, QTextEdit, QPlainTextEdit, QComboBox, QSpinBox {
  background: {{SURFACE}};
  border: 1px solid {{BORDER}};
  border-radius: {{RADIUS_MEDIUM}}px;
  padding: 8px 10px;
  color: {{TEXT_PRIMARY}};
  selection-background-color: {{ACCENT}};
  selection-color: {{ACCENT_TEXT}};
}
QLineEdit:focus, QTextEdit:focus, QPlainTextEdit:focus, QComboBox:focus, QSpinBox:focus {
  border: 1px solid {{ACCENT}};
}
QLineEdit[invalid="true"] { border: 1px solid {{DANGER}}; }

QComboBox::drop-down { border: none; width: 24px; }

QPushButton {
  background: {{SURFACE}};
  border: 1px solid {{BORDER}};
  border-radius: {{RADIUS_MEDIUM}}px;
  padding: 8px 16px;
  color: {{TEXT_PRIMARY}};
  font-weight: 500;
}
QPushButton:hover:enabled { background: {{SURFACE_SUNKEN}}; }
QPushButton:pressed:enabled { background: {{BORDER}}; }
QPushButton:disabled { color: {{TEXT_SECONDARY}}; }

QPushButton[accent="true"] {
  background: {{ACCENT}};
  border: 1px solid {{ACCENT}};
  color: {{ACCENT_TEXT}};
  font-weight: 600;
}
QPushButton[accent="true"]:hover:enabled { background: {{HOVER_ACCENT}}; border: 1px solid {{HOVER_ACCENT}}; }
QPushButton[accent="true"]:pressed:enabled { background: {{PRESSED_ACCENT}}; border: 1px solid {{PRESSED_ACCENT}}; }
QPushButton[accent="true"]:disabled { background: {{BORDER}}; border: 1px solid {{BORDER}}; color: {{TEXT_SECONDARY}}; }

QPushButton[flat="true"] { background: transparent; border: none; }

QPushButton[role="pill"] {
  background: transparent;
  border: 1px solid {{BORDER}};
  border-radius: {{RADIUS_PILL}}px;
  padding: 6px 14px;
  color: {{TEXT_SECONDARY}};
  font-weight: 500;
}
QPushButton[role="pill"]:hover:enabled {
  background: {{SURFACE_SUNKEN}};
  color: {{TEXT_PRIMARY}};
  border-color: {{BORDER}};
}
QPushButton[role="pill"]:checked {
  background: {{ACCENT}};
  border: 1px solid {{ACCENT}};
  color: {{ACCENT_TEXT}};
  font-weight: 600;
}
QPushButton[role="pill"]:checked:hover:enabled {
  background: {{HOVER_ACCENT}};
  border: 1px solid {{HOVER_ACCENT}};
}

QAbstractButton[role="styleCard"], QPushButton[role="styleCard"], QToolButton[role="styleCard"] {
  background: {{SURFACE}};
  border: 1px solid {{BORDER}};
  border-radius: {{RADIUS_MEDIUM}}px;
  padding: 8px 12px;
  color: {{TEXT_PRIMARY}};
  font-weight: 500;
}
QAbstractButton[role="styleCard"]:hover:enabled, QPushButton[role="styleCard"]:hover:enabled, QToolButton[role="styleCard"]:hover:enabled {
  background: {{SURFACE_SUNKEN}};
  border-color: {{ACCENT}};
}
QAbstractButton[role="styleCard"]:checked, QPushButton[role="styleCard"]:checked, QToolButton[role="styleCard"]:checked {
  background: {{SURFACE}};
  border: 2px solid {{ACCENT}};
  color: {{ACCENT}};
  font-weight: 600;
}

QToolButton[role="topBarButton"], QPushButton[role="topBarButton"] {
  background: transparent;
  border: none;
  border-radius: {{RADIUS_SMALL}}px;
  padding: 6px 8px;
  color: {{TEXT_PRIMARY}};
}
QToolButton[role="topBarButton"]:hover:enabled, QPushButton[role="topBarButton"]:hover:enabled {
  background: {{SURFACE_SUNKEN}};
}
QToolButton[role="topBarButton"]:pressed:enabled, QPushButton[role="topBarButton"]:pressed:enabled {
  background: {{BORDER}};
}

QToolButton[role="clearButton"], QPushButton[role="clearButton"] {
  background: transparent;
  border: none;
  border-radius: 10px;
  padding: 2px;
  color: {{TEXT_TERTIARY}};
}
QToolButton[role="clearButton"]:hover:enabled, QPushButton[role="clearButton"]:hover:enabled {
  color: {{TEXT_PRIMARY}};
  background: {{SURFACE_SUNKEN}};
}

QCheckBox, QRadioButton { spacing: 8px; }
QCheckBox::indicator, QRadioButton::indicator { width: 16px; height: 16px; }

QScrollArea { border: none; background: transparent; }
QScrollBar:vertical { width: 10px; background: transparent; margin: 0; }
QScrollBar::handle:vertical { background: {{BORDER}}; border-radius: 5px; min-height: 24px; }
QScrollBar::add-line, QScrollBar::sub-line { height: 0; }

QToolTip {
  background: {{SURFACE}};
  border: 1px solid {{BORDER}};
  color: {{TEXT_PRIMARY}};
  padding: 4px 6px;
}
)");

  qss.replace(QStringLiteral("{{FONT}}"), systemFontFamily());
  qss.replace(QStringLiteral("{{TEXT_PRIMARY}}"), palette.textPrimary.name());
  qss.replace(QStringLiteral("{{TEXT_SECONDARY}}"), palette.textSecondary.name());
  qss.replace(QStringLiteral("{{TEXT_TERTIARY}}"), palette.textTertiary.name());
  qss.replace(QStringLiteral("{{WINDOW}}"), palette.window.name());
  qss.replace(QStringLiteral("{{SURFACE}}"), palette.surface.name());
  qss.replace(QStringLiteral("{{SURFACE_SUNKEN}}"), palette.surfaceSunken.name());
  qss.replace(QStringLiteral("{{BORDER}}"), palette.border.name());
  qss.replace(QStringLiteral("{{SUCCESS}}"), palette.success.name());
  qss.replace(QStringLiteral("{{DANGER}}"), palette.danger.name());
  qss.replace(QStringLiteral("{{ACCENT}}"), accent);
  qss.replace(QStringLiteral("{{ACCENT_TEXT}}"), accentText);
  qss.replace(QStringLiteral("{{HOVER_ACCENT}}"), hoverAccent);
  qss.replace(QStringLiteral("{{PRESSED_ACCENT}}"), pressedAccent);
  qss.replace(QStringLiteral("{{RADIUS_SMALL}}"), QString::number(Tokens::RadiusSmall));
  qss.replace(QStringLiteral("{{RADIUS_MEDIUM}}"), QString::number(Tokens::RadiusMedium));
  qss.replace(QStringLiteral("{{RADIUS_LARGE}}"), QString::number(Tokens::RadiusLarge));
  qss.replace(QStringLiteral("{{RADIUS_CARD}}"), QString::number(Tokens::RadiusCard));
  qss.replace(QStringLiteral("{{RADIUS_PILL}}"), QString::number(Tokens::RadiusPill));

  return qss;
}

QString systemFontFamily() { return PlatformIntegration::systemFontFamily(); }
