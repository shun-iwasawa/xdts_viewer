#pragma once
#ifndef DIALOGS_H
#define DIALOGS_H

#include <QDialog>
#include <QMap>
#include <QStringList>

class QComboBox;
class QLineEdit;
class QLabel;
class QColorDialog;
class QFontComboBox;
class QCheckBox;
class QGroupBox;
class QRadioButton;

class SettingsDialog : public QDialog {
  Q_OBJECT
  // format
  QComboBox* m_templateCombo;
  QGroupBox* m_expandColumnsGB;
  QLineEdit* m_cameraColumnAdditionEdit;
  QComboBox* m_mixUpColumnsCombo;
  QPushButton* m_mixupKeyBtn;
  QLineEdit* m_logoImgPathField;
  QComboBox* m_exportAreaCombo;
  QLineEdit* m_skippedLevelNamesEdit;
  QCheckBox* m_withDenpyoCB;
  QCheckBox* m_showSkippedDrawingsCB;
  QLineEdit* m_backsideImgPathField;
  QGroupBox* m_scannedGengaSheetGB;
  QLineEdit* m_gengaLevelsCountEdit;
  // QLineEdit* m_dougaColumnOffsetEdit; // deprecated
  QLineEdit* m_scannedSheetPageAmountEdit;
  QLabel* m_pageInfoLbl;

  QComboBox* m_startOLSecCombo;
  QComboBox* m_startOLKomaCombo;
  QComboBox* m_endOLSecCombo;
  QComboBox* m_endOLKomaCombo;

  int m_duration;
  //  QStringList m_levelNames;

public:
  SettingsDialog(QWidget* parent);
  // SettingsDialog(QWidget* parent, const QStringList& levelNames, int
  // duration);
  void syncUIs();
  void setDuration(int duration) { m_duration = duration; }

protected slots:
  void onTemplateSwitched(int);
  void onExpandColumnsSwitched();
  void onMixUpActivated();
  void openMixupKeyDialog();
  void onFormatSettingsChanged();
  void onLogoImgBrowserButtonClicked();
  void onBacksideImgBrowserButtonClicked();
  void onSkippedLevelNameChanged();
  void onGengaLevelsCountEdited();
  void onCameraColumnAdditionEdited();
  void onStartOLComboChanged(int);
  void onEndOLComboChanged(int);
  void onDenpyoCheckboxClicked(bool);
  void onShowSkippedDrawingsClicked(bool);
};

class PreferencesDialog : public QDialog {
  Q_OBJECT

  // user
  QComboBox* m_languageCombo;
  QPushButton* m_lineColorButton;
  QColorDialog* m_lineColorDialog;
  // QFontComboBox* m_templateFontCB, * m_contentsFontCB;
  QComboBox* m_continuousLineCombo;
  QLineEdit* m_minimumRepeatLengthField;
  QCheckBox *m_serialFrameNumberCB, *m_levelNameOnBottomCB,
      *m_capitalizeFirstLetterCB;
  QLineEdit* m_stampFolderPathField;
  QLineEdit* m_approvalNameField;
  QLineEdit* m_emptyFrameForAEField;
  QMap<int, QLineEdit*> m_suffixEdits;

public:
  PreferencesDialog(QWidget* parent);
  void syncUIs();
protected slots:
  void onLanguageSwitched();
  void onViewPreferencesChanged();
  void onLineColorChanged(const QColor&);
  void updateLineColorButton();
  void onStampBrowserButtonClicked();
  void onStampPathChanged();
  void onApprovalNameChanged();
  void onEmptyFrameForAEChanged();
  void onCapitalizeFirstLetterSwitched();
  void onSuffixEdited();
};

// Lets the user choose how to sync a Clip Studio Paint timesheet export
// (always written to the same fixed path) with a real XDTS file: reuse an
// already-open window, or sync/export to a file path they enter (an
// existing path is overwritten in place; a not-yet-existing path is
// exported as new — decided from disk state at OK time, not from how the
// path was entered). Used only by InstanceManager (the mothership process).
// Clip Studio Paintのタイムシート書き出し（常に同一の固定パスに書かれる）を
// 実体のあるXDTSファイルと同期する方法をユーザーに選ばせるダイアログ。
// 既に開いているウィンドウの再利用、または入力したパスへの同期／新規
// エクスポート（既存パスならその場で上書き、未存在のパスなら新規
// エクスポート——OKクリック時のディスク上の状態で判定し、入力方法では
// 区別しない）から選ぶ。InstanceManager（母艦プロセス）専用。
class CspLinkChooserDialog : public QDialog {
  Q_OBJECT

  QRadioButton* m_useOpenRadio;
  QComboBox* m_openFilesCombo;
  QRadioButton* m_useFileRadio;
  QLineEdit* m_filePathField;

  QString m_resultPath;
  bool m_needsOverwriteConfirm;

public:
  CspLinkChooserDialog(const QStringList& openPaths, QWidget* parent = nullptr);

  // Valid only once exec() returns QDialog::Accepted.
  QString resultPath() const { return m_resultPath; }
  bool needsOverwriteConfirm() const { return m_needsOverwriteConfirm; }

protected slots:
  void onBrowseFile();
  void onAccept();
};

class AboutDialog : public QDialog {
  Q_OBJECT

public:
  AboutDialog(QWidget* parent);

protected:
  void paintEvent(QPaintEvent*) override;
};
#endif