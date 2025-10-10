#pragma once
#ifndef XSHEETPREVIEWAREA_H
#define XSHEETPREVIEWAREA_H

#include <QWidget>
#include <QScrollArea>
#include <QMarginsF>
#include <QSize>
#include <QPoint>
#include <QPainter>
#include <QList>
#include <QSet>
#include <QPageSize>
#include <QPdfWriter>
#include <QMap>
#include <iostream>

#include "myparams.h"

class XsheetPdfPreviewArea;
// parameters which can be defined in template files
namespace XSheetPDFTemplateParamIDs {
// numbers
const std::string BodyAmount            = "BodyAmount";
const std::string KeyColAmount          = "KeyColAmount";
const std::string CellsColAmount        = "CellsColAmount";
const std::string CameraColAmount       = "CameraColAmount";
const std::string FrameLength           = "FrameLength";
const std::string MemoLinesAmount       = "MemoLinesAmount";
const std::string ExtraCellsColAmount   = "ExtraCellsColAmount";
const std::string DrawCameraGrid        = "DrawCameraGrid";
const std::string DrawCameraHeaderGrid  = "DrawCameraHeaderGrid";
const std::string DrawCameraHeaderLabel = "DrawCameraHeaderLabel";
const std::string DrawCellsHeaderLabel  = "DrawCellsHeaderLabel";
const std::string DrawKeysHeaderLabel   = "DrawKeysHeaderLabel";
const std::string DrawDialogHeaderLine  = "DrawDialogHeaderLine";
const std::string TranslateBodyLabel    = "TranslateBodyLabel";
const std::string TranslateInfoLabel    = "TranslateInfoLabel";
const std::string IsBlockBorderThick    = "IsBlockBorderThick";

// lengths
const std::string BodyWidth        = "BodyWidth";
const std::string BodyHeight       = "BodyHeight";
const std::string BodyHMargin      = "BodyHMargin";
const std::string BodyTop          = "BodyTop";
const std::string HeaderHeight     = "HeaderHeight";
const std::string KeyColWidth      = "KeyColWidth";
const std::string LastKeyColWidth  = "LastKeyColWidth";
const std::string DialogColWidth   = "DialogColWidth";
const std::string CellsColWidth    = "CellsColWidth";
const std::string CameraColWidth   = "CameraColWidth";
const std::string RowHeight        = "RowHeight";
const std::string OneSecHeight     = "1SecHeight";
const std::string InfoOriginLeft   = "InfoOriginLeft";
const std::string InfoOriginTop    = "InfoOriginTop";
const std::string InfoTitleHeight  = "InfoTitleHeight";
const std::string InfoBodyHeight   = "InfoBodyHeight";
const std::string ThinLineWidth    = "ThinLineWidth";
const std::string ThickLineWidth   = "ThickLineWidth";
const std::string BodyOutlineWidth = "BodyOutlineWidth";
};  // namespace XSheetPDFTemplateParamIDs

// ids for various information area
enum XSheetPDFDataType {
  Data_Memo = 0,
  Data_Second,
  Data_Frame,
  Data_OverlapLength,
  Data_TotalPages,
  Data_CurrentPage,
  Data_DateTimeAndScenePath,
  Data_SceneName,
  Data_Logo,
  Data_Invalid
};

typedef void (*DecoFunc)(QPainter&, QRect, QMap<XSheetPDFDataType, QRect>&,
                         bool);

enum TickMarkType {
  TickMark_Dot = 0,
  TickMark_Circle,
  TickMark_Filled,
  TickMark_Asterisk
};

struct XSheetPDFFormatInfo {
  QColor lineColor;
  QString dateTimeText;
  QString scenePathText;
  QString sceneNameText;
  QList<ExportArea> exportAreas;
  QString templateFontFamily;
  QString contentsFontFamily;
  QString memoText;
  QString logoText;
  QPixmap logoPixmap;
  bool drawSound;
  bool serialFrameNumber;
  bool drawLevelNameOnBottom;
  ContinuousLineMode continuousLineMode;
  int tick1MarkId;
  int tick2MarkId;
  int keyMarkId;
  TickMarkType tick1MarkType;
  TickMarkType tick2MarkType;
  bool expandColumns;
  int startOverlapFrameLength;
  int endOverlapFrameLength;
};

class XSheetPDFTemplate {
protected:
  struct XSheetPDF_InfoFormat {
    int width;
    QString label;
    DecoFunc decoFunc = nullptr;
  };

  struct XSheetPDFTemplateParams {
    QPageSize::PageSizeId documentPageSize;
    QMarginsF documentMargin;
    QList<XSheetPDF_InfoFormat> array_Infos;
    int bodylabelTextSize_Large;
    int bodylabelTextSize_Small;
    int keyBlockWidth;
    int cellsBlockWidth;
    int cameraBlockWidth;
    int infoHeaderHeight;
    int cellColumnOffset;
    int cameraColumnAdditionWidth;
  } m_p;

  QMap<std::string, int> m_params;

  QPen thinPen, thickPen, blockBorderPen, bodyOutlinePen;

  XSheetPDFFormatInfo m_info;

  QMap<ExportArea, QList<QList<QRect>>> m_colLabelRects;
  QMap<ExportArea, QList<QList<QRect>>> m_colLabelRects_bottom;
  QMap<ExportArea, QList<QList<QRect>>> m_cellRects;
  QMap<ExportArea, QList<QRect>> m_bodyBBoxes;

  QList<QRect> m_soundCellRects;
  QMap<XSheetPDFDataType, QRect> m_dataRects;
  QList<QRect> m_cameraCellRects;

  // column and column name (if manually specified)
  QMap<ExportArea, ColumnsData> m_columns;
  // QList<QPair<QVector<QString>, QString>> m_columns;
  // QList<TXshSoundColumn*> m_soundColumns;
  // TXshSoundTextColumn* m_noteColumn;
  struct TerekoInfo {
    QString levelName;
    QSet<int> colIdSet;
    QList<int> occupiedColIds;
  };
  QMap<ExportArea, QList<TerekoInfo>> m_terekoColumns;
  // QMap<ExportArea, QMap<QString, QList<int> >> m_terekoColumns;

  struct RepeatInfo {
    int repeatChunkStartFrame;  // チャンクのスタート位置
    int repeatChunkLength;      // チャンクの長さ
    int repeatEndFrame;         // 繰り返しの終わりフレーム
  };

  // QMap<int,QList<RepeatInfo>> → colId ごとのリピート情報
  QMap<ExportArea, QMap<int, QList<RepeatInfo>>> m_repeatColumns;

  QString m_skippedDrawingText;

  int m_duration;
  int m_longestDuration;  // 一番長い列の長さ
  bool m_useExtraColumns;

  void adjustSpacing(QPainter& painter, const int width, const QString& label,
                     const double ratio = 0.8);

  void drawGrid(QPainter& painter, int colAmount, int colWidth, int blockWidth);
  void drawHeaderGrid(QPainter& painter, int colAmount, int colWidth,
                      int blockWidth);

  void registerColLabelRects(QPainter& painter, int colAmount, int colWidth,
                             int bodyId, ExportArea area);
  void registerCellRects(QPainter& painter, int colAmount, int colWidth,
                         int bodyId, ExportArea area);
  void registerSoundRects(QPainter& painter, int colWidth, int bodyId);
  void registerCameraRects(QPainter& painter, int colAmount, int colWidth);

  // Key Block
  void drawKeyBlock(QPainter& painter, int framePage, const int bodyId);

  void drawDialogBlock(QPainter& painter, const int framePage,
                       const int bodyId);

  void drawCellsBlock(QPainter& painter, int bodyId);

  void drawCameraBlock(QPainter& painter, const int bodyId);

  void drawXsheetBody(QPainter& painter, int framePage, int bodyId);

  void drawInfoHeader(QPainter& painter);

  void addInfo(int w, QString lbl, DecoFunc f = nullptr);

  void drawContinuousLine(QPainter& painter, QRect rect, bool isEmpty);
  void drawRepeatLine(QPainter& painter, QRect rect);
  void drawCellNumber(QPainter& painter, QRect rect, FrameData& cellFdata,
                      QString terekoColName);
  // void drawCellNumber(QPainter& painter, QRect rect, QString& cellFid,
  //   bool isKey);
  // void drawCellNumber(QPainter& painter, QRect rect, TXshCell& cell,
  //   bool isKey);
  void drawTickMark(QPainter& painter, QRect rect, TickMarkType type,
                    QString terekoColName);
  void drawEndMark(QPainter& painter, QRect upperRect);
  void drawLevelName(QPainter& painter, QRect rect, QString name,
                     bool isBottom = false);
  void drawTerekoArrow(QPainter& painter, QRect prevRect, QRect rect);
  void drawOLMark(QPainter& painter, QRect rect, double topRatio,
                  double bottomRatio);

  void drawLogo(QPainter& painter);
  // void drawSound(QPainter& painter, int framePage);
  // void drawDialogue(QPainter& painter, int framePage);

  int param(const std::string& id, int defaultValue = 0);

  int getStretchedColumnWidth();

  void setPenVisible(bool show);

  // drawXsheetContentsから呼ばれる。テレコ情報を踏まえて
  // 各表示列の各フレームでの重ね順を記録する
  QList<QList<int>> getOccupiedColumns(ExportArea area, int startColId,
                                       int colsInPage, int startFrame);

public:
  XSheetPDFTemplate(const QMap<ExportArea, ColumnsData>& columns,
                    const int duration);
  void drawXsheetTemplate(QPainter& painter, int framePage,
                          bool isPreview = false);
  void drawXsheetContents(QPainter& painter, int framePage, int prallelPage,
                          bool isPreview = false);
  void initializePage(QPdfWriter& writer);
  QPixmap initializePreview();
  int framePageCount();
  int parallelPageCount();
  int columnsInPage(ExportArea area);
  QSize logoPixelSize();
  void setLogoPixmap(QPixmap pm);
  // void setSoundColumns(const QList<TXshSoundColumn*>& soundColumns) {
  //   m_soundColumns = soundColumns;
  //}
  // void setNoteColumn(TXshSoundTextColumn* noteColumn) {
  //  m_noteColumn = noteColumn;
  //}
  void setInfo(const XSheetPDFFormatInfo& info);

  void checkTerekoColumns();
  void checkMixupColumnsKeyframes();
  void checkRepeatColumns();
  void checkSkippedDrawings();

  QSize getPixelSize();

  // AETool, DyeTool用
  // QMap<ExportArea, QList<QList<QRect>>> m_colLabelRects;
  // QMap<ExportArea, QList<QList<QRect>>> m_cellRects;
  QList<QList<QRect>> colLabelRects(ExportArea area) {
    return m_colLabelRects.value(area, QList<QList<QRect>>());
  }
  QList<QList<QRect>> cellRects(ExportArea area) {
    return m_cellRects.value(area, QList<QList<QRect>>());
  }
  QList<QRect> bodyBBoxes(ExportArea area) {
    return m_bodyBBoxes.value(area, QList<QRect>());
  }

  QMap<ExportArea, ColumnsData> columns() { return m_columns; }
  bool hasTerekoColumns(ExportArea area) {
    return !m_terekoColumns.value(area, QList<TerekoInfo>()).isEmpty();
  }

  int framePerPage() { return param(XSheetPDFTemplateParamIDs::FrameLength); }
  int keyColumnAmountTmpl() {
    return m_params.value(XSheetPDFTemplateParamIDs::KeyColAmount);
  }
  int cellsColumnAmountTmpl() {
    return m_params.value(XSheetPDFTemplateParamIDs::CellsColAmount);
  }
};

class XSheetPDFTemplate_B4_6sec : public XSheetPDFTemplate {
public:
  XSheetPDFTemplate_B4_6sec(const QMap<ExportArea, ColumnsData>& columns,
                            const int duration);
};

class XSheetPDFTemplate_Custom : public XSheetPDFTemplate {
  bool m_valid;

public:
  XSheetPDFTemplate_Custom(const QString& fp,
                           const QMap<ExportArea, ColumnsData>& columns,
                           const int duration);
  bool isValid() { return m_valid; }
};

//-----------------------------------------------------------------------------

class XsheetPdfPreviewPane final : public QWidget {
  Q_OBJECT
  double m_scaleFactor;
  QPixmap m_pixmap;
  QPixmap m_scaledPixmap;

  QImage m_scribbleImage;
  QPixmap m_scaledScribblePixmap;

  QImage m_canvasImage;
  QPointF m_canvasPos;

  QPainter m_canvasPainter;
  XsheetPdfPreviewArea* m_area;

  bool m_isTablet;
  bool m_scribbling;
  QPointF m_mousePos;

  bool m_isCanvasValid;

  QRect m_clipRect;

  void computeScaledPixmap();
  void computeScaledScribblePixmap();

protected:
  void paintEvent(QPaintEvent* event) override;
  void mousePressEvent(QMouseEvent* e) override;
  void mouseMoveEvent(QMouseEvent* e) override;
  void mouseReleaseEvent(QMouseEvent* e) override;
  void tabletEvent(QTabletEvent* event) override;
  void leaveEvent(QEvent* event) override;
  void enterEvent(QEvent* event) override;

public:
  XsheetPdfPreviewPane(QWidget* parent);
  void setPixmap(QPixmap pm);
  double doZoom(bool zoomIn);
  void fitScaleTo(QSize size);
  void setScribbleImage(QImage img) {
    m_scribbleImage        = img;
    m_scaledScribblePixmap = QPixmap();
  }
  QImage scribbleImage() { return m_scribbleImage; }
  void setScribbleMode(bool isErase);

  QRect drawLine(const QPointF& from, const QPointF& to, qreal pressure,
                 bool isErase, double brushSize);
  double scaleFactor() { return m_scaleFactor; }
  void setArea(XsheetPdfPreviewArea* area) { m_area = area; }
  QPointF mouseCoord() const { return m_mousePos / m_scaleFactor; }
  void invalidateCanvas() { m_isCanvasValid = false; }

  void setClipRect(QRect rect) {
    if (m_clipRect.isNull())
      m_clipRect = rect;
    else
      m_clipRect = m_clipRect.united(rect);
  }
  void releaseClipRect() { m_clipRect = QRect(); }
signals:
  void imageEdited();
public slots:
  void updateCanvas();
};

class XsheetPdfPreviewArea final : public QScrollArea {
  Q_OBJECT
  XsheetPdfPreviewPane* m_pane;

protected:
  void contextMenuEvent(QContextMenuEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;

public:
  XsheetPdfPreviewArea(QWidget* parent);
  void setPane(XsheetPdfPreviewPane* pane) {
    setWidget(pane);
    m_pane = pane;
  }
  // QPointF mousePos() { return m_mousePos; }
  // bool scribbling() { return m_scribbling; }
protected slots:
  void fitToWindow();
  void onSliderActionTriggered();
  void zoomIn();
  void zoomOut();
};

//-----------------------------------------------------------------------------

#endif