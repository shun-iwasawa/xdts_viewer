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

public:
  MyWindow();
  ~MyWindow();
  void initialize();
  bool askAndSaveChanges();
  void setPage(int page);
  int duration();

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
  void onCut();
  void onCopy();
  void onPaste();
  void onDelete();
  void onBacksideImgPathChanged();
  void onAbout();
};

#endif