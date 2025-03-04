#pragma once
#ifndef MIXUPKEYDIALOG_H
#define MIXUPKEYDIALOG_H

#include "myparams.h"
#include <QDialog>
#include <QTableWidget>
#include <QStyledItemDelegate>
#include <QProxyStyle>
#include <QMap>

class QPushButton;
class QCheckBox;
class QStackedWidget;
class QTabBar;

class MixupTableStyle : public QProxyStyle {
public:
  MixupTableStyle(QStyle* style = 0) : QProxyStyle(style) {}

  void drawPrimitive(PrimitiveElement element, const QStyleOption* option,
                     QPainter* painter,
                     const QWidget* widget = 0) const override;
};

//=============================================================================

class MixupTableDelegate : public QStyledItemDelegate {
  Q_OBJECT

public:
  MixupTableDelegate(QObject* parent) : QStyledItemDelegate(parent) {}
  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override;
  QSize sizeHint(const QStyleOptionViewItem& option,
                 const QModelIndex& index) const override;
  void setModelData(QWidget* editor, QAbstractItemModel* model,
                    const QModelIndex& index) const override;
  QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem& option,
                        const QModelIndex& index) const override;
signals:
  void editingFinished(const QModelIndex&) const;
};

//=============================================================================

class MixupTable : public QTableWidget {
  Q_OBJECT

  QStringList m_colNames;
  ExportArea m_area;

  QRect m_dropIndicatorRect;

public:
  MixupTable(ExportArea area, QStringList colNames, QWidget* parent = nullptr);
  void updateTable();
  int colIdFromIndex(const QModelIndex& index) const;
  QString labelFromIndex(const QModelIndex& index) const;
  bool isIndexSelected(const QModelIndex& index) const;

  QRect dropIndicatorRect() const { return m_dropIndicatorRect; }

  QColor getColumnColor(const QModelIndex& index, bool isSelected) const;
  ExportArea area() const { return m_area; }
  void setArea(const ExportArea area) { m_area = area; }

protected:
  void dragEnterEvent(QDragEnterEvent* e) override;
  void dropEvent(QDropEvent* e) override;
  void dragMoveEvent(QDragMoveEvent* e) override;

protected slots:
  void onItemEdited(const QModelIndex&);
};

class MixupKeyDialog : public QDialog {
  Q_OBJECT

  QMap<ExportArea, MixupTable*> m_tables;

  // キーフレームが原画、動画で異なる場合用
  QCheckBox* m_shareKeysCheckbox;
  QStackedWidget* m_tableStack;
  QTabBar* m_tableTab;

  QPushButton *m_addKeyButton, *m_deleteKeyButton;

  MixupTable* currentTable();

public:
  MixupKeyDialog(QWidget* parent   = nullptr,
                 Qt::WindowFlags f = Qt::WindowFlags());
protected slots:
  void onAddKey();
  void onDeleteKey();
  void onSelectionChanged();

  void onShareKeysClicked(bool);
};

#endif