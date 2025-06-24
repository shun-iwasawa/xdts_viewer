#include "dyetool.h"
#include "xsheetpreviewarea.h"

DyeUndo::DyeUndo(XsheetPdfPreviewPane* view) : m_view(view), m_onPush(true) {
  m_before.insert(Area_Cells, DyedCellsData());
  m_before.insert(Area_Actions, DyedCellsData());
  m_after.insert(Area_Cells, DyedCellsData());
  m_after.insert(Area_Actions, DyedCellsData());
}

void DyeUndo::dye(ExportArea area, QPair<int, int> pos, QColor color) {
  // 通過済みならreturn
  if (m_after[area].contains(pos)) return;

  DyedCellsData dyedCells = MyParams::instance()->dyedCells(area);
  if (dyedCells.value(pos, QColor(Qt::black)) == color) return;

  m_before[area].insert(pos, dyedCells.value(pos, QColor(Qt::black)));
  m_after[area].insert(pos, color);

  if (color == QColor(Qt::black))
    MyParams::instance()->dyedCells(area).remove(pos);
  else
    MyParams::instance()->dyedCells(area).insert(pos, color);
}

void DyeUndo::undo() {
  for (int area = Area_Cells; area <= Area_Actions; area++) {
    DyedCellsData data = m_before[(ExportArea)area];
    for (auto pos : data.keys()) {
      if (data.value(pos) == QColor(Qt::black))
        MyParams::instance()->dyedCells((ExportArea)area).remove(pos);
      else
        MyParams::instance()
            ->dyedCells((ExportArea)area)
            .insert(pos, data.value(pos));
    }
  }
  MyParams::instance()->notifyTemplateSwitched();
  MyParams::instance()->setFormatDirty(true);
}

void DyeUndo::redo() {
  if (m_onPush) {
    m_onPush = false;
    return;
  }
  for (int area = Area_Cells; area <= Area_Actions; area++) {
    DyedCellsData data = m_after[(ExportArea)area];
    for (auto pos : data.keys()) {
      if (data.value(pos) == QColor(Qt::black))
        MyParams::instance()->dyedCells((ExportArea)area).remove(pos);
      else
        MyParams::instance()
            ->dyedCells((ExportArea)area)
            .insert(pos, data.value(pos));
    }
  }
  MyParams::instance()->notifyTemplateSwitched();
  MyParams::instance()->setFormatDirty(true);
}

bool DyeUndo::isEmpty() {
  return m_before[Area_Cells].isEmpty() && m_before[Area_Actions].isEmpty();
}

//=======================================================================

QPair<ExportArea, QPoint> DyeTool::posToCell(QPointF pos) {
  QPair<ExportArea, QPoint> ret = {Area_Unspecified, QPoint()};

  int currentPage = MyParams::instance()->currentPage();
  if (currentPage == 0) return ret;

  QPoint _pos = (pos / m_view->scaleFactor()).toPoint();
  // 現在マウスカーソルがあるセル位置を得る
  XSheetPDFTemplate* tmpl = MyParams::instance()->currentTemplate();

  for (int area = Area_Cells; area <= Area_Actions; area++) {
    QList<QRect> bodyBBoxes = tmpl->bodyBBoxes((ExportArea)area);
    bool foundInBody        = false;
    for (auto bodyBBox : bodyBBoxes) {
      if (bodyBBox.contains(_pos)) {
        foundInBody = true;
        break;
      }
    }
    if (!foundInBody) continue;

    ret.first  = (ExportArea)area;
    ret.second = QPoint(-1, -1);

    QList<QList<QRect>> cellRects = tmpl->cellRects((ExportArea)area);
    for (int c = 0; c < cellRects.size(); c++) {
      for (int r = 0; r < cellRects[c].size(); r++) {
        if (cellRects[c][r].contains(_pos)) {
          ret.second = QPoint(c, r);
          break;
        }
      }
      if (ret.second != QPoint(-1, -1)) break;
    }

    assert(ret.second != QPoint(-1, -1));
    break;
  }

  return ret;
}

QPair<ExportArea, QPoint> DyeTool::cellToSheetCoord(
    QPair<ExportArea, QPoint> cell) {
  if (cell.first == Area_Unspecified) return cell;

  int currentPage         = MyParams::instance()->currentPage();
  XSheetPDFTemplate* tmpl = MyParams::instance()->currentTemplate();
  int parallelPageCount   = tmpl->parallelPageCount();

  int fPage = (currentPage - 1) / parallelPageCount;
  int pPage = (currentPage - 1) % parallelPageCount;

  int startFrame = tmpl->framePerPage() * fPage;

  // 表示可能列が小さい方に合わせる
  QMap<ExportArea, ColumnsData> columns = tmpl->columns();
  int colsInPage                        = 1000;
  for (auto area : columns.keys())
    colsInPage = std::min(colsInPage, tmpl->columnsInPage(area));
  int startColId = colsInPage * pPage;

  return {cell.first, cell.second + QPoint(startColId, startFrame)};
}

DyeTool::DyeTool()
    : Tool(Tool_Dye)
    , m_currentCell({Area_Unspecified, QPoint()})
    , m_undo(nullptr) {}

void DyeTool::onPress(const QPointF& pos, Qt::KeyboardModifiers modifiers) {
  if (m_undo) delete m_undo;

  m_undo = new DyeUndo(m_view);

  QPair<ExportArea, QPoint> sheetCoord = cellToSheetCoord(posToCell(pos));

  if (sheetCoord.first == Area_Unspecified) return;

  m_undo->dye(sheetCoord.first, {sheetCoord.second.x(), sheetCoord.second.y()},
              MyParams::instance()->currentColor());
}

void DyeTool::onDrag(const QPointF& from, const QPointF& to,
                     Qt::KeyboardModifiers modifiers, qreal pressure) {
  if (!m_undo) return;

  QPair<ExportArea, QPoint> sheetCoord = cellToSheetCoord(posToCell(to));

  if (sheetCoord.first == Area_Unspecified) return;

  m_undo->dye(sheetCoord.first, {sheetCoord.second.x(), sheetCoord.second.y()},
              MyParams::instance()->currentColor());
}

void DyeTool::onRelease(const QPointF& pos, QImage& canvasImg,
                        const QPointF& canvasPos) {
  if (!m_undo) return;

  if (m_undo->isEmpty()) {
    delete m_undo;
    m_undo = nullptr;
    return;
  }

  MyParams::instance()->undoStack()->push(m_undo);

  m_undo = nullptr;
  MyParams::instance()->notifyTemplateSwitched();
  MyParams::instance()->setFormatDirty(true);
}

void DyeTool::onMove(const QPointF& pos) { m_currentCell = posToCell(pos); }

void DyeTool::draw(QPainter& painter, QPointF pos, double scaleFactor) {
  int currentPage = MyParams::instance()->currentPage();
  if (currentPage == 0) return;

  XSheetPDFTemplate* tmpl = MyParams::instance()->currentTemplate();

  int parallelPageCount = tmpl->parallelPageCount();

  int fPage = (currentPage - 1) / parallelPageCount;
  int pPage = (currentPage - 1) % parallelPageCount;

  int startFrame = tmpl->framePerPage() * fPage;
  int endFrame   = tmpl->framePerPage() * (fPage + 1);

  // 表示可能列が小さい方に合わせる
  QMap<ExportArea, ColumnsData> columns = tmpl->columns();
  int colsInPage                        = 1000;
  for (auto area : columns.keys())
    colsInPage = std::min(colsInPage, tmpl->columnsInPage(area));
  int startColId = colsInPage * pPage;
  int endColId   = colsInPage * (pPage + 1);

  painter.save();

  painter.setCompositionMode(QPainter::CompositionMode_Multiply);

  QPixmap pm(painter.device()->width(), painter.device()->height());
  pm.fill(Qt::transparent);
  QPainter pmp(&pm);
  pmp.scale(scaleFactor, scaleFactor);

  for (int area = Area_Cells; area <= Area_Actions; area++) {
    QList<QRect> bodyBBoxes = tmpl->bodyBBoxes((ExportArea)area);
    // 薄く現在の文字色を塗る。基本は黒
    for (auto bodybbox : bodyBBoxes) pmp.fillRect(bodybbox, Qt::black);

    // 現在のセル色情報を反映する
    DyedCellsData dyedCells = MyParams::instance()->dyedCells((ExportArea)area);
    QList<QList<QRect>> cellRects = tmpl->cellRects((ExportArea)area);

    for (auto key : dyedCells.keys()) {
      // keyが現在のページに含まれているかチェック
      if (startColId <= key.first && key.first < endColId &&
          startFrame <= key.second && key.second < endFrame) {
        int c = key.first - startColId;
        int r = key.second - startFrame;

        QColor color = dyedCells.value(key);
        pmp.fillRect(cellRects[c][r], color);
      }
    }

    // カーソルのセル
    if (m_currentCell.first == (ExportArea)area) {
      pmp.fillRect(
          cellRects[m_currentCell.second.x()][m_currentCell.second.y()],
          MyParams::instance()->currentColor());
    }
  }
  painter.setOpacity(0.3);
  painter.drawPixmap(QPoint(), pm);

  painter.restore();
}

void DyeTool::onDeactivate() {
  if (m_view) m_view->update();
}

bool DyeTool::onActivate() {
  if (m_view) m_view->update();
  return true;
}

DyeTool _dyeTool;