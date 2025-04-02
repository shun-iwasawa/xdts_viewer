#include "tool.h"
#include "xsheetpreviewarea.h"

#include <QSettings>

LineTool::LineTool() : Tool(Tool_Line), m_lineId(ThinLine) {}

void LineTool::onPress(const QPointF& pos, Qt::KeyboardModifiers modifiers) {
  m_dragStartPos = (pos / m_view->scaleFactor()).toPoint();
  m_currentPos   = m_dragStartPos;
}

void LineTool::onDrag(const QPointF& from, const QPointF& to,
                      Qt::KeyboardModifiers modifiers, qreal pressure) {
  if (m_dragStartPos.isNull()) return;

  QPoint pos = (to / m_view->scaleFactor()).toPoint();
  if (modifiers & Qt::ShiftModifier) {
    QPoint vec = pos - m_dragStartPos;
    double rad = std::atan2((double)vec.y(), (double)vec.x()) + M_PI * 2.;
    rad        = M_PI_4 * std::round(rad / M_PI_4);
    double d   = std::sqrt((double)(vec.x() * vec.x() + vec.y() * vec.y()));
    pos        = QPoint(d * cos(rad), d * sin(rad)) + m_dragStartPos;
  }
  m_currentPos = pos;
}

void LineTool::onRelease(const QPointF& pos, QImage& canvasImg,
                         const QPointF& canvasPos) {
  QRect bbox = QRect(m_dragStartPos, m_currentPos).normalized();
  int margin = 10;
  bbox.adjust(-margin, -margin, margin, margin);
  bbox = bbox.intersected(QRect(QPoint(), m_view->scribbleImage().size()));

  QImage imageBefore = m_view->scribbleImage().copy(bbox);

  QImage img = m_view->scribbleImage();
  QPainter p(&img);
  p.setRenderHint(QPainter::Antialiasing);
  doDrawLine(p);
  p.end();
  m_view->setScribbleImage(img);

  MyParams::instance()->undoStack()->push(new BrushUndo(
      imageBefore, m_view->scribbleImage().copy(bbox), bbox, m_view));
  m_dragStartPos = QPoint();
}

void LineTool::draw(QPainter& p, QPointF pos, double scaleFactor) {
  p.save();
  p.scale(scaleFactor, scaleFactor);
  if (m_dragStartPos.isNull()) return;
  doDrawLine(p);
  p.restore();
}

void LineTool::saveToolState(QSettings& settings) {
  settings.setValue("LineTool_Type", (int)m_lineId);
}

void LineTool::loadToolState(QSettings& settings) {
  m_lineId = (LineId)settings.value("LineTool_Type", (int)m_lineId).toInt();
}

void LineTool::doDrawLine(QPainter& p) {
  static const QPointF points[3] = {QPointF(0.0, 0.0), QPointF(-15.0, 8.0),
                                    QPointF(-15.0, -8.0)};
  switch (m_lineId) {
  case ThinLine:
    p.setPen(QPen(MyParams::instance()->currentColor(), 1.5));
    p.drawLine(m_dragStartPos, m_currentPos);
    break;
  case ThickLine:
    p.setPen(QPen(MyParams::instance()->currentColor(), 5.));
    p.drawLine(m_dragStartPos, m_currentPos);
    break;
  case SmallArrow: {
    QPoint vec = m_currentPos - m_dragStartPos;
    double deg = std::atan2((double)vec.y(), (double)vec.x()) * 180. / M_PI;
    double d   = std::sqrt((double)(vec.x() * vec.x() + vec.y() * vec.y()));
    p.save();
    p.translate(m_currentPos);
    p.rotate(deg);
    p.setPen(QPen(MyParams::instance()->currentColor(), 1.5));
    p.drawLine(QPointF(-10, 0), QPointF(-d, 0.));
    p.setPen(Qt::NoPen);
    p.setBrush(MyParams::instance()->currentColor());
    p.drawPolygon(points, 3);
    p.restore();
  } break;

  case LargeArrow: {
    QPoint vec = m_currentPos - m_dragStartPos;
    double deg = std::atan2((double)vec.y(), (double)vec.x()) * 180. / M_PI;
    double d   = std::sqrt((double)(vec.x() * vec.x() + vec.y() * vec.y()));
    p.save();
    p.translate(m_currentPos);
    p.rotate(deg);
    p.setPen(QPen(MyParams::instance()->currentColor(), 5.));
    p.drawLine(QPointF(-10, 0), QPointF(-d, 0.));
    p.setPen(Qt::NoPen);
    p.setBrush(MyParams::instance()->currentColor());
    p.scale(2., 2.);
    p.drawPolygon(points, 3);
    p.restore();
  } break;
  case DoubledLine: {
    p.setPen(QPen(MyParams::instance()->currentColor(), 1.5));
    QPoint vec = m_currentPos - m_dragStartPos;
    double deg = std::atan2((double)vec.y(), (double)vec.x()) * 180. / M_PI;
    double d   = std::sqrt((double)(vec.x() * vec.x() + vec.y() * vec.y()));
    p.save();
    p.translate(m_currentPos);
    p.rotate(deg);
    p.drawLine(QPointF(0., 4.), QPointF(-d, 4.));
    p.drawLine(QPointF(0, -4.), QPointF(-d, -4.));
    p.restore();
  } break;
  case DottedLine:
    QPen pen(MyParams::instance()->currentColor(), 2., Qt::CustomDashLine);
    pen.setDashPattern({3., 4.});
    p.setPen(pen);
    p.drawLine(m_dragStartPos, m_currentPos);
    break;
  }
}

LineTool _lineTool;