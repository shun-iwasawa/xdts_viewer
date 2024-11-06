#pragma once
#ifndef AETOOL_H
#define AETOOL_H

#include "tool.h"

class QPropertyAnimation;

class AETool : public Tool {
  int m_currentColumnId;
  QPropertyAnimation* m_animation;

public:
  AETool();
  void setView(XsheetPdfPreviewPane* view) override;
  void onPress(const QPointF& pos, Qt::KeyboardModifiers modifiers) override;
  void onMove(const QPointF& pos) override;
  void draw(QPainter& painter, QPointF pos, double scaleFactor) override;
  bool onActivate() override;
};

#endif