#pragma once

#ifndef DYETOOL_H
#define DYETOOL_H

#include "tool.h"
#include <QPair>
#include <QUndoCommand>

class DyeUndo : public QUndoCommand {
  QMap<ExportArea, DyedCellsData> m_before, m_after;
  XsheetPdfPreviewPane* m_view;
  bool m_onPush;

public:
  DyeUndo(XsheetPdfPreviewPane* view);

  void dye(ExportArea area, QPair<int, int> pos, QColor color);
  void undo() override;
  void redo() override;

  bool isEmpty();
};

class DyeTool : public Tool {
  QPair<ExportArea, QPoint> m_currentCell;

  DyeUndo* m_undo;

  QPair<ExportArea, QPoint> posToCell(QPointF pos);
  QPair<ExportArea, QPoint> cellToSheetCoord(QPair<ExportArea, QPoint> cell);

public:
  DyeTool();
  void onPress(const QPointF& pos, Qt::KeyboardModifiers modifiers) override;
  void onDrag(const QPointF& from, const QPointF& to,
              Qt::KeyboardModifiers modifiers, qreal pressure) override;
  void onRelease(const QPointF& pos, QImage& canvasImg,
                 const QPointF& canvasPos) override;
  void onMove(const QPointF& pos) override;
  void draw(QPainter& painter, QPointF pos, double scaleFactor) override;
  void onDeactivate() override;
  bool onActivate() override;
};

#endif