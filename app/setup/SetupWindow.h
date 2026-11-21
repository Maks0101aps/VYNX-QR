#pragma once
#include <QWidget>
#include "SetupBackend.h"

class QLabel;
class QPushButton;
class QLineEdit;
class QStackedWidget;
class QProgressBar;
class QCheckBox;
class QComboBox;
class QCloseEvent;

class SetupWindow : public QWidget {
  Q_OBJECT
public:
  explicit SetupWindow(const QString &payload, QWidget *parent = nullptr);
  int step() const { return step_; }
protected:
  void closeEvent(QCloseEvent *event) override;
private:
  void setStep(int step);
  void advance();
  void retranslateUi();

  SetupBackend backend_;
  int step_ = 0;
  bool busy_ = false;
  QStackedWidget *pages_;
  QLabel *steps_[4];
  QLineEdit *folder_;
  QLabel *pathError_, *status_, *error_, *destination_;
  QProgressBar *progress_;
  QPushButton *next_, *back_, *cancel_;
  QCheckBox *launch_;
  QComboBox *langCombo_ = nullptr;
  QLabel *welcomeTitle_ = nullptr;
  QLabel *welcomeSub_ = nullptr;
  QLabel *welcomeDesc_ = nullptr;
  QLabel *folderTitle_ = nullptr;
  QLabel *folderSub_ = nullptr;
  QLabel *folderLabel_ = nullptr;
  QPushButton *browseBtn_ = nullptr;
  QLabel *licenseFootnote_ = nullptr;
  QPushButton *licenseBtn_ = nullptr;
  QLabel *finishTitle_ = nullptr;
  QLabel *finishSub_ = nullptr;
  QLabel *versionLabel_ = nullptr;
};
