#include "tool.h"

#include "xsheetpreviewarea.h"

#include <QUndoCommand>

Tool::Tool(ToolId id) : m_view(nullptr) {
  MyParams::instance()->registerTool(id, this);
}

void Tool::updateView() { m_view->update(); }

//---------------------------------------

BrushUndo::BrushUndo(const QImage& before, const QImage& after,
                     const QRect& boundingBox, XsheetPdfPreviewPane* view)
    : m_before(before)
    , m_after(after)
    , m_boundingBox(boundingBox)
    , m_view(view)
    , m_onPush(true) {
  m_page = MyParams::instance()->currentPage();
}

void BrushUndo::setImage(bool isUndo) {
  // 現在のページがカレントかどうかで上書きする相手を変える
  if (m_page == MyParams::instance()->currentPage()) {
    QImage img = m_view->scribbleImage();
    QPainter p(&img);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.drawImage(m_boundingBox, (isUndo) ? m_before : m_after);
    p.end();
    m_view->setScribbleImage(img);
    m_view->update();
  } else {
    QImage img = MyParams::instance()->scribbleImage(m_page);
    QPainter p(&img);
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.drawImage(m_boundingBox, (isUndo) ? m_before : m_after);
    p.end();
    MyParams::instance()->setScribbleImage(m_page, img);
  }
  MyParams::instance()->setImageDirty(m_page);
}

void BrushUndo::undo() { setImage(true); }

void BrushUndo::redo() {
  if (m_onPush) {
    m_onPush = false;
    return;
  }
  setImage(false);
}
//---------------------------------------

BrushTool::BrushTool() : Tool(Tool_Brush), m_lastPos() { setSize(Small); }

void BrushTool::onPress(const QPointF& pos, Qt::KeyboardModifiers modifiers) {
  m_imageBefore = m_view->scribbleImage().copy();
  // draw straight line from the last position
  if (modifiers & Qt::ShiftModifier) {
    m_boundingBox = m_view->drawLine(m_lastPos, pos, 0.5, false, m_brushSize);
  } else
    m_boundingBox = QRect();
  // std::cout << "start" << std::endl;
}

void BrushTool::onDrag(const QPointF& from, const QPointF& to,
                       Qt::KeyboardModifiers modifiers, qreal pressure) {
  QRect rect = m_view->drawLine(from, to, pressure, false, m_brushSize);

  // std::cout << "from : " << from.x() << ", " << from.y() << "   to : " <<
  // to.x() << ", " << to.y() << std::endl;
  m_boundingBox = (m_boundingBox.isNull()) ? rect : m_boundingBox.united(rect);

  // m_view->setClipRect(rect);
}

void BrushTool::onRelease(const QPointF& pos, QImage& canvasImg,
                          const QPointF& canvasPos) {
  QImage img = m_view->scribbleImage();
  QPainter p(&img);
  p.drawImage(canvasPos, canvasImg);
  p.end();
  m_view->setScribbleImage(img);
  canvasImg.fill(Qt::transparent);

  int margin = (int)(m_brushSize / 2.) + 3;
  m_boundingBox.adjust(-margin, -margin, margin, margin);
  m_boundingBox = m_boundingBox.intersected(
      QRect(QPoint(), m_view->scribbleImage().size()));

  MyParams::instance()->undoStack()->push(new BrushUndo(
      m_imageBefore.copy(m_boundingBox),
      m_view->scribbleImage().copy(m_boundingBox), m_boundingBox, m_view));

  m_lastPos = pos;
  // std::cout << "end"<<std::endl;
}

void BrushTool::setSize(BrushSize sizeId) {
  m_brushSize = (sizeId == Large) ? 30. : 4.;
}

bool BrushTool::onActivate() {
  m_view->setScribbleMode(false);
  return true;
}

void BrushTool::onEnter() {
  m_view->setCursor(MyParams::instance()->getBrushToolCursor());
}

//---------------------------------------
EraserTool::EraserTool() : Tool(Tool_Eraser) {}

void EraserTool::onPress(const QPointF& pos, Qt::KeyboardModifiers modifiers) {
  m_imageBefore = m_view->scribbleImage().copy();
  m_boundingBox = QRect();
}

void EraserTool::onDrag(const QPointF& from, const QPointF& to,
                        Qt::KeyboardModifiers modifiers, qreal pressure) {
  QRect rect    = m_view->drawLine(from, to, pressure, true, 60.);
  m_boundingBox = (m_boundingBox.isNull()) ? rect : m_boundingBox.united(rect);
}

void EraserTool::onRelease(const QPointF& pos, QImage& canvasImg,
                           const QPointF& canvasPos) {
  QImage img = m_view->scribbleImage();
  QPainter p(&img);
  p.setCompositionMode(QPainter::CompositionMode_DestinationOut);
  p.drawImage(canvasPos, canvasImg);
  p.end();
  m_view->setScribbleImage(img);
  canvasImg.fill(Qt::transparent);

  m_boundingBox.adjust(-33, -33, 33, 33);
  m_boundingBox = m_boundingBox.intersected(
      QRect(QPoint(), m_view->scribbleImage().size()));

  MyParams::instance()->undoStack()->push(new BrushUndo(
      m_imageBefore.copy(m_boundingBox),
      m_view->scribbleImage().copy(m_boundingBox), m_boundingBox, m_view));
  m_imageBefore = QImage();
}

void EraserTool::draw(QPainter& painter, QPointF pos, double scaleFactor) {
  // if (m_imageBefore.isNull()) return;
  painter.setPen(Qt::blue);
  // std::cout << pos.x() << ",  " << pos.y() << std::endl;
  painter.drawEllipse(pos, 30. * scaleFactor, 30. * scaleFactor);
}

bool EraserTool::onActivate() {
  m_view->setScribbleMode(true);
  return true;
}

//---------------------------------------

StampTool::StampTool() : Tool(Tool_Stamp), m_stampId(0), m_validFlag(false) {}

void StampTool::onPress(const QPointF& pos, Qt::KeyboardModifiers modifiers) {
  // クリック時の挙動作る
  QPixmap stampPm = m_stampImgs[m_stampId].second;
  QPointF p       = pos / m_view->scaleFactor();
  int margin      = 3;
  QRect boundingBox(p.x() - stampPm.width() / 2, p.y() - stampPm.height() / 2,
                    stampPm.width(), stampPm.height());
  boundingBox.adjust(-margin, -margin, margin, margin);
  boundingBox =
      boundingBox.intersected(QRect(QPoint(), m_view->scribbleImage().size()));

  QImage img       = m_view->scribbleImage();
  QImage imgBefore = img.copy(boundingBox);

  QPainter painter(&img);
  painter.setCompositionMode(QPainter::CompositionMode_Multiply);
  painter.setRenderHints(QPainter::Antialiasing |
                         QPainter::SmoothPixmapTransform);
  painter.drawPixmap(p.x() - stampPm.width() / 2, p.y() - stampPm.height() / 2,
                     stampPm);
  painter.end();
  m_view->setScribbleImage(img);

  MyParams::instance()->undoStack()->push(
      new BrushUndo(imgBefore, m_view->scribbleImage().copy(boundingBox),
                    boundingBox, m_view));

  MyParams::instance()->setImageDirty(MyParams::instance()->currentPage());
}

void StampTool::draw(QPainter& painter, QPointF pos, double scaleFactor) {
  QPixmap scaledStampPixmap = prepareScaledStamp(scaleFactor);
  painter.drawPixmap(pos.x() - scaledStampPixmap.width() / 2,
                     pos.y() - scaledStampPixmap.height() / 2,
                     scaledStampPixmap);
}

void StampTool::registerStamp(const QString name, const QPixmap pm) {
  m_stampImgs.append(QPair<QString, QPixmap>(name, pm));
}

QPixmap& StampTool::prepareScaledStamp(double scaleFactor) {
  static QPixmap scaledStampPixmap;
  if (m_validFlag && scaleFactor == m_tmpScaleFactor) return scaledStampPixmap;

  QPixmap pm        = m_stampImgs[m_stampId].second;
  scaledStampPixmap = pm.scaled(pm.size() * scaleFactor, Qt::KeepAspectRatio,
                                Qt::SmoothTransformation);
  m_tmpScaleFactor  = scaleFactor;
  return scaledStampPixmap;
}

//---------------------------------------
BrushTool _brushTool;
EraserTool _eraserTool;
StampTool _stampTool;