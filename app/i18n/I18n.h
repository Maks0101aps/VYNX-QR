#pragma once

#include <QString>

class I18n {
public:
  enum class Language {
    English,
    Russian,
    Ukrainian
  };

  static I18n &instance();

  void setLanguage(Language lang);
  [[nodiscard]] Language language() const;

  [[nodiscard]] QString translate(const char *sourceText) const;

  static QString tr(const char *sourceText) {
    return instance().translate(sourceText);
  }

private:
  I18n();
  Language language_ = Language::English;
};
