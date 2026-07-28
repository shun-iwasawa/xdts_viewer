#pragma once

#ifndef MYWINDOW_H
#define MYWINDOW_H

#include "myparams.h"

#include <QMainWindow>

class QLineEdit;
class QPushButton;

class XdtsData;
class XsheetPdfPreviewPane;
class XsheetPdfPreviewArea;
class XSheetPDFTemplate;
class SettingsDialog;
class PreferencesDialog;
class QActionGroup;

class MyWindow : public QMainWindow {
  Q_OBJECT

  XsheetPdfPreviewPane* m_previewPane;
  XsheetPdfPreviewArea* m_previewArea;

  QLineEdit* m_currentPageEdit;
  int m_totalPageCount;
  QPushButton *m_prev, *m_next;

  QMap<ExportArea, ColumnsData> m_columns;
  // QList<QPair<QVector<QString>, QString>> m_columns;

  QMap<ExportArea, int> m_durations;
  XdtsData* m_data;

  SettingsDialog* m_settingsDialog;
  PreferencesDialog* m_preferencesDialog;

  QActionGroup* m_toolActionGroup;

  QAction* getToolAction(ToolId);

  QAction* m_currentColorAction;

  // CLIP STUDIO PAINT integration UI state.
  // CLIP STUDIO PAINT連携用のUI状態。
  QAction* m_unlinkCspAct;
  bool m_isCspLinked;

  // Re-reads the XDTS timing data only, leaving hand-drawn overlays / undo
  // history / format settings untouched. Used by reloadAndActivate().
  // 手描きオーバーレイ・Undo履歴・フォーマット設定はそのままに、XDTSの
  // タイミングデータのみ再読み込みする。reloadAndActivate() から呼ばれる。
  void reloadXdtsDataOnly();

public:
  MyWindow();
  ~MyWindow();
  void initialize();
  bool askAndSaveChanges();
  void setPage(int page);
  int duration();

  // Requested by InstanceManager when another launch asks to open the file
  // that is already open in this window: reloads the XDTS data (preserving
  // unsaved hand-drawn edits) and brings the window to the foreground.
  // 別プロセスの起動が、このウィンドウで既に開いているファイルを開こうとした
  // 際にInstanceManagerから呼ばれる。XDTSデータを再読み込みし（未保存の手描き
  // データは保持したまま）、ウィンドウを前面化する。
  void reloadAndActivate();

  // Updates the title bar / Unlink menu visibility to reflect the current
  // CSP link status (InstanceManager::cspLinkStatusChanged()).
  // 現在のCSP連携の紐づけ状態を、タイトルバーとUnlinkメニューの表示に
  // 反映する（InstanceManager::cspLinkStatusChanged()）。
  void onCspLinkStatusChanged(bool linked);

protected:
  void closeEvent(QCloseEvent* event) override;

  void dragEnterEvent(QDragEnterEvent* event) override;
  void dropEvent(QDropEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

protected slots:
  void initTemplate();
  void setInfo(bool forExportImage = false);
  void updatePreview();
  void onPrev();
  void onNext();
  // void onCurrentPageEdited();
  void onImageEdited();
  void onToolActionTriggered(QAction*);
  void onToolSwitched();
  void onCurrentColorActionTriggered(QAction*);
  void onBrushSizeActionTriggered(QAction*);
  void onSelectionModeActionTriggered(QAction*);
  void onLineTypeActionTriggered(QAction*);
  void onStampTypeActionTriggered(QAction*);
  void onUndo();
  void onRedo();

  void onLoad(const QString& xdtsPath);
  void onLoad() { onLoad(QString()); }

  void updateTitleBar();
  void onSave();
  void onExport();
#ifdef WIN32
  void onExportPSD(QString fileName);
#endif
  void onCut();
  void onCopy();
  void onPaste();
  void onDelete();
  void onBacksideImgPathChanged();
  void onAbout();

  // Explicitly unlinks this window from a Clip Studio Paint sync, if it
  // is currently the linked window (see InstanceManager::requestUnlink()).
  // このウィンドウがClip Studio Paint連携の紐づけ先である場合、明示的に
  // 紐づけを解除する（InstanceManager::requestUnlink()を参照）。
  void onUnlinkCsp();
};

#endif