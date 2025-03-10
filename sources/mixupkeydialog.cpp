#include "mixupkeydialog.h"

#include "myparams.h"
#include "xsheetpreviewarea.h"
#include "myparams.h"

#include <QMessageBox>
#include <QHeaderView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include <QDropEvent>
#include <QLineEdit>
#include <QIntValidator>
#include <QMessageBox>
#include <QLabel>
#include <QCheckBox>
#include <QStackedWidget>

namespace {

const QColor colItemBgColor(230, 240, 225);
const QColor mixedColItemBgColor(130, 190, 255);

const QColor selectedColItemBgColor(255, 255, 175);
const QColor firstColItemBgColor(180, 180, 180);
}  // namespace

//=============================================================================
// FrameItem
//-----------------------------------------------------------------------------

class FrameItem final : public QTableWidgetItem {
  int m_frame;
  bool m_isFirst;

public:
  FrameItem(int frame, bool isFirst = false)
      : QTableWidgetItem(QString::number(frame + 1), UserType + 1)
      , m_frame(frame)
      , m_isFirst(isFirst) {
    if (isFirst)
      setFlags(Qt::NoItemFlags);
    else
      setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);
  }

  int frame() const { return m_frame; }
};
//=============================================================================
// ColumnItem
//-----------------------------------------------------------------------------

class ColumnItem final : public QTableWidgetItem {
  int m_colId;
  bool m_isFirst;

public:
  ColumnItem(QString name, int colId, bool isFirst = false)
      : QTableWidgetItem(name, UserType), m_colId(colId), m_isFirst(isFirst) {
    if (isFirst)
      setFlags(Qt::NoItemFlags);
    else
      setFlags(Qt::ItemIsSelectable | Qt::ItemIsDragEnabled |
               Qt::ItemIsEnabled);

    // setBackground(isFirst ? firstColItemBgColor : colItemBgColor);
  }

  int colId() const { return m_colId; }
};

//=============================================================================

void MixupTableStyle::drawPrimitive(PrimitiveElement element,
                                    const QStyleOption* option,
                                    QPainter* painter,
                                    const QWidget* widget) const {
  if (element == QStyle::PE_IndicatorItemViewItemDrop &&
      !option->rect.isNull()) {
    QStyleOption opt(*option);
    if (widget) {
      const MixupTable* table = dynamic_cast<const MixupTable*>(widget);
      opt.rect                = table->dropIndicatorRect();
    }
    QProxyStyle::drawPrimitive(element, &opt, painter, widget);
    return;
  }
  QProxyStyle::drawPrimitive(element, option, painter, widget);
}

//=============================================================================

void MixupTableDelegate::paint(QPainter* painter,
                               const QStyleOptionViewItem& option,
                               const QModelIndex& index) const {
  const MixupTable* table = dynamic_cast<const MixupTable*>(option.widget);
  if (!table) return;

  bool isSelected = table->isIndexSelected(index);

  QString label = table->labelFromIndex(index);
  painter->save();
  // frames
  if (index.column() == 0) {
    painter->setPen(Qt::black);
    painter->setBrush((index.row() == 0) ? Qt::lightGray : Qt::white);
    painter->drawText(option.rect, Qt::AlignCenter, label);

  }
  // columns
  else {
    painter->setPen(Qt::black);
    painter->setBrush(table->getColumnColor(index, isSelected));
    QRect boxRect = option.rect.marginsRemoved(QMargins(2, 2, 2, 2));
    painter->drawRoundedRect(boxRect, 5, 3);
    painter->drawText(boxRect, Qt::AlignCenter, label);
    // QStyledItemDelegate::paint(painter, option, index);
  }
  painter->restore();
}

QSize MixupTableDelegate::sizeHint(const QStyleOptionViewItem& option,
                                   const QModelIndex& index) const {
  QSize ret = QStyledItemDelegate::sizeHint(option, index);
  ret.setHeight(18);
  return ret;
}

void MixupTableDelegate::setModelData(QWidget* editor,
                                      QAbstractItemModel* model,
                                      const QModelIndex& index) const {
  QStyledItemDelegate::setModelData(editor, model, index);
  if (index.column() == 0) emit editingFinished(index);
}

QWidget* MixupTableDelegate::createEditor(QWidget* parent,
                                          const QStyleOptionViewItem& option,
                                          const QModelIndex& index) const {
  if (index.column() != 0 && index.row() == 0) return nullptr;

  QWidget* editor = QStyledItemDelegate::createEditor(parent, option, index);

  QIntValidator* validator = new QIntValidator();
  validator->setBottom(2);
  qobject_cast<QLineEdit*>(editor)->setValidator(validator);

  return editor;
}

//=============================================================================

MixupTable::MixupTable(ExportArea area, QStringList colNames, QWidget* parent)
    : QTableWidget(parent), m_area(area), m_colNames(colNames) {
  MixupTableDelegate* delegate = new MixupTableDelegate(this);
  setItemDelegate(delegate);

  setStyle(new MixupTableStyle(style()));

  setDragEnabled(true);
  setShowGrid(false);
  setDropIndicatorShown(true);
  setDefaultDropAction(Qt::MoveAction);
  setDragDropOverwriteMode(false);
  setDragDropMode(QAbstractItemView::InternalMove);
  setSelectionMode(QAbstractItemView::SingleSelection);
  setDropIndicatorShown(true);
  setMouseTracking(true);
  horizontalHeader()->hide();
  horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  horizontalHeader()->setStretchLastSection(false);
  verticalHeader()->hide();

  updateTable();

  connect(delegate, SIGNAL(editingFinished(const QModelIndex&)), this,
          SLOT(onItemEdited(const QModelIndex&)));
}

void MixupTable::updateTable() {
  QMap<int, QList<int>> keyframes =
      MyParams::instance()->mixUpColumnsKeyframes(m_area);
  clear();
  setColumnCount(m_colNames.size() + 1);
  setRowCount(keyframes.size() + 1);

  setItem(0, 0, new FrameItem(0));
  horizontalHeader()->resizeSection(0, 30);
  int col = 1;
  for (auto colName : m_colNames) {
    setItem(0, col, new ColumnItem(colName, col - 1, true));
    horizontalHeader()->resizeSection(col, 30);
    col++;
  }
  verticalHeader()->resizeSection(0, 22);

  int row = 1;
  for (auto frame : keyframes.keys()) {
    QList<int> colIndices = keyframes.value(frame);

    setItem(row, 0, new FrameItem(frame));

    int col = 1;
    for (auto colId : colIndices) {
      setItem(row, col, new ColumnItem(m_colNames[colId], colId));
      col++;
    }

    verticalHeader()->resizeSection(row, 22);
    row++;
  }
}

int MixupTable::colIdFromIndex(const QModelIndex& index) const {
  QTableWidgetItem* item = itemFromIndex(index);
  ColumnItem* colItem    = dynamic_cast<ColumnItem*>(item);
  if (!colItem) return -1;

  return colItem->colId();
}

QString MixupTable::labelFromIndex(const QModelIndex& index) const {
  QTableWidgetItem* item = itemFromIndex(index);
  if (!item) return QString();
  return item->text();
}

bool MixupTable::isIndexSelected(const QModelIndex& index) const {
  QTableWidgetItem* item = itemFromIndex(index);
  return (item) ? item->isSelected() : false;
}

QColor MixupTable::getColumnColor(const QModelIndex& index,
                                  bool isSelected) const {
  if (index.row() == 0) return firstColItemBgColor;
  if (isSelected) return selectedColItemBgColor;
  int row                = index.row() - 1;
  int col                = index.column();
  QModelIndex upperIndex = index.siblingAtRow(index.row() - 1);
  const ColumnItem* upperItem =
      dynamic_cast<const ColumnItem*>(itemFromIndex(upperIndex));
  const ColumnItem* item =
      dynamic_cast<const ColumnItem*>(itemFromIndex(index));

  return (upperItem->colId() == item->colId()) ? colItemBgColor
                                               : mixedColItemBgColor;
}

void MixupTable::dragEnterEvent(QDragEnterEvent* e) {
  // 同じRowでないとドロップできないようにする
  int dragItemRow = selectedIndexes()[0].row();

  for (int row = 1; row < rowCount(); row++) {
    for (int col = 1; col < columnCount(); col++) {
      QTableWidgetItem* itm = item(row, col);
      if (dragItemRow != 0 && row == dragItemRow)
        itm->setFlags(itm->flags() | Qt::ItemIsDropEnabled);
      else
        itm->setFlags(itm->flags() & ~Qt::ItemIsDropEnabled);
    }
  }
  QTableWidget::dragEnterEvent(e);
}

void MixupTable::dropEvent(QDropEvent* e) {
  QItemSelectionModel* modelSelection = selectionModel();

  QModelIndex dstIndex = indexAt(e->pos());

  QTableWidgetItem* itm = item(dstIndex.row(), dstIndex.column());
  if (!itm) return;
  QRect itmRect = visualItemRect(itm);

  int dstColumn =
      ((e->pos().x() - itmRect.left()) < (itmRect.right() - e->pos().x()))
          ? dstIndex.column()
          : dstIndex.column() + 1;

  QMap<int, QList<int>> mixUpColumnsKeyframes =
      MyParams::instance()->mixUpColumnsKeyframes(m_area);
  int srcColumn   = selectedIndexes()[0].column();
  int dragItemRow = selectedIndexes()[0].row();

  int frame = dynamic_cast<FrameItem*>(item(dragItemRow, 0))->frame();
  if (!mixUpColumnsKeyframes.contains(frame)) return;

  QList<int> keyFrame = mixUpColumnsKeyframes.value(frame);

  int moveFrom = srcColumn - 1;
  int moveTo   = std::max(0, dstColumn - 1);
  if (moveFrom < moveTo) moveTo -= 1;
  keyFrame.move(moveFrom, moveTo);

  MyParams::instance()->setMixupColumnsKeyframe(m_area, frame, keyFrame);

  updateTable();
  MyParams::instance()->notifyTemplateSwitched();
  MyParams::instance()->setFormatDirty(true);
}

void MixupTable::dragMoveEvent(QDragMoveEvent* e) {
  QModelIndex dstIndex = indexAt(e->pos());

  int col = dstIndex.column();
  col     = std::min(std::max(col, 1), columnCount() - 1);

  QTableWidgetItem* itm = item(dstIndex.row(), col);
  if (!itm) return;
  QRect itmRect = visualItemRect(itm);

  int xPos;
  if ((e->pos().x() - itmRect.left()) < (itmRect.right() - e->pos().x()))
    xPos = itmRect.left();
  else
    xPos = itmRect.right();

  m_dropIndicatorRect = QRect(xPos - 2, itmRect.top(), 5, itmRect.height());

  QTableWidget::dragMoveEvent(e);
}

void MixupTable::onItemEdited(const QModelIndex& index) {
  if (index.column() != 0) return;

  const FrameItem* frameItem =
      dynamic_cast<const FrameItem*>(itemFromIndex(index));

  int frameBefore = frameItem->frame();
  int frameAfter  = frameItem->text().toInt() - 1;

  if (frameBefore == frameAfter) return;

  QMap<int, QList<int>> mixupKeyframes =
      MyParams::instance()->mixUpColumnsKeyframes(m_area);

  assert(mixupKeyframes.contains(frameBefore));

  QList<int> keyframe = mixupKeyframes.value(frameBefore);

  MyParams::instance()->deleteMixUpColumnsKeyframeAt(m_area, frameBefore);
  MyParams::instance()->setMixupColumnsKeyframe(m_area, frameAfter, keyframe);

  updateTable();

  MyParams::instance()->notifyTemplateSwitched();
  MyParams::instance()->setFormatDirty(true);
}

//---------------------------------------------------------------

MixupKeyDialog::MixupKeyDialog(QWidget* parent, Qt::WindowFlags f)
    : QDialog(parent, f)
    , m_shareKeysCheckbox(nullptr)
    , m_tableStack(nullptr)
    , m_tableTab(nullptr) {
  setModal(true);
  setWindowTitle(tr("Edit Mix-up Columns Keyframes"));

  m_addKeyButton    = new QPushButton(this->tr("Add"), this);
  m_deleteKeyButton = new QPushButton(this->tr("Delete"), this);

  QString labelStr = this->tr(
      "- Press \"Add\" button to add a new keyframe.\n"
      "- Double-click on the leftmost column to edit the frame.\n"
      "- Drag and drop column name labels to reorder them.");

  QMap<ExportArea, ColumnsData> columns =
      MyParams::instance()->currentTemplate()->columns();
  QMap<ExportArea, QStringList> colNames;

  for (auto area : columns.keys()) {
    ColumnsData columnsData = columns.value(area);
    QStringList names;
    for (auto columnData : columnsData) names.append(columnData.name);

    if (columns.size() == 1) area = Area_Unspecified;

    MixupTable* table = new MixupTable(area, names, this);
    m_tables.insert(area, table);
    colNames.insert(area, names);
    connect(table, SIGNAL(itemSelectionChanged()), this,
            SLOT(onSelectionChanged()));
  }

  // 原画欄/動画欄に両方データがある場合
  if (columns.size() == 2) {
    // 表示セルが一式同じならキーフレームをシェア可能
    if (colNames.values()[0] == colNames.values()[1])
    {
      m_shareKeysCheckbox =
          new QCheckBox(tr("Share Keyframes Among Areas"), this);
      m_shareKeysCheckbox->setChecked(
          MyParams::instance()->isMixUpColumnsKeyframesShared());

      this->connect(m_shareKeysCheckbox, SIGNAL(clicked(bool)), this,
                    SLOT(onShareKeysClicked(bool)));
    }

    m_tableStack = new QStackedWidget(this);
    m_tableStack->addWidget(m_tables.value(Area_Actions));
    m_tableStack->addWidget(m_tables.value(Area_Cells));

    m_tableTab = new QTabBar(this);
    m_tableTab->addTab(tr("ACTIONS"));
    m_tableTab->addTab(tr("CELLS"));

    connect(m_tableTab, SIGNAL(tabBarClicked(int)), m_tableStack,
            SLOT(setCurrentIndex(int)));

    // シェアしているならタブを非表示にする
    if (m_shareKeysCheckbox && m_shareKeysCheckbox->isChecked()) {
      m_tableStack->setCurrentIndex(0);
      m_tableTab->hide();
    }
  }

  QVBoxLayout* mainLay = new QVBoxLayout();
  mainLay->setMargin(10);
  mainLay->setSpacing(10);
  {
    // 原画欄/動画欄に両方データがある場合
    if (columns.size() == 1)
      mainLay->addWidget(m_tables.values()[0], 1);
    else if (columns.size() == 2) {
      QVBoxLayout* tableLay = new QVBoxLayout();
      tableLay->setMargin(0);
      tableLay->setSpacing(0);

      tableLay->addWidget(m_tableTab, 0);
      tableLay->addWidget(m_tableStack, 1);

      mainLay->addLayout(tableLay, 1);
      if (m_shareKeysCheckbox) mainLay->addWidget(m_shareKeysCheckbox, 0);
    }

    QHBoxLayout* buttonsLay = new QHBoxLayout();
    buttonsLay->setMargin(10);
    buttonsLay->setSpacing(10);
    {
      buttonsLay->addWidget(new QLabel(labelStr, this), 0);
      buttonsLay->addStretch(1);
      buttonsLay->addWidget(m_addKeyButton, 0);
      buttonsLay->addWidget(m_deleteKeyButton, 0);
    }
    mainLay->addLayout(buttonsLay, 0);
  }
  setLayout(mainLay);

  connect(m_addKeyButton, SIGNAL(clicked()), this, SLOT(onAddKey()));
  connect(m_deleteKeyButton, SIGNAL(clicked()), this, SLOT(onDeleteKey()));
}

MixupTable* MixupKeyDialog::currentTable() {
  if (m_tables.size() == 1) return m_tables.values()[0];
  return qobject_cast<MixupTable*>(m_tableStack->currentWidget());
}

void MixupKeyDialog::onAddKey() {
  MixupTable* table = currentTable();
  ExportArea area   = table->area();
  int frame         = 1;
  QMap<int, QList<int>> mixupColumnsKeyframes =
      MyParams::instance()->mixUpColumnsKeyframes(area);

  if (!mixupColumnsKeyframes.isEmpty())
    frame = mixupColumnsKeyframes.keys().last() + 1;

  int columnCount = table->columnCount() - 1;
  QList<int> keyframeData;
  for (int c = 0; c < columnCount; c++) keyframeData.append(c);

  MyParams::instance()->setMixupColumnsKeyframe(area, frame, keyframeData);

  table->updateTable();
  MyParams::instance()->notifyTemplateSwitched();
  MyParams::instance()->setFormatDirty(true);
}

void MixupKeyDialog::onDeleteKey() {
  MixupTable* table              = currentTable();
  ExportArea area                = table->area();
  QList<QTableWidgetItem*> items = table->selectedItems();
  if (items.isEmpty()) return;
  int row = table->row(items[0]);
  if (row == 0) return;

  const FrameItem* frameItem =
      dynamic_cast<const FrameItem*>(table->item(row, 0));
  int frame = frameItem->frame();

  // 確認ダイアログ
  QString questionMsg =
      tr("Deleting Mix-up Columns Keyframe at Frame %1. Are You Sure?")
          .arg(QString::number(frame + 1));
  QMessageBox::StandardButton ret =
      QMessageBox::question(this, tr("Question"), questionMsg);
  if (ret != QMessageBox::Yes) return;

  MyParams::instance()->deleteMixUpColumnsKeyframeAt(area, frame);
  table->updateTable();
  MyParams::instance()->notifyTemplateSwitched();
  MyParams::instance()->setFormatDirty(true);
}

void MixupKeyDialog::onSelectionChanged() {
  MixupTable* table              = currentTable();
  QList<QTableWidgetItem*> items = table->selectedItems();
  if (items.isEmpty() || table->row(items[0]) == 0)
    m_deleteKeyButton->setDisabled(true);
  else
    m_deleteKeyButton->setEnabled(true);
}

void MixupKeyDialog::onShareKeysClicked(bool share) {
  if (share) {
    // ONにするとき、双方のキーフレームが異なる場合に一方（動画欄）を消してもよいか確認する
    if (MyParams::instance()->mixUpColumnsKeyframes(Area_Actions) !=
            MyParams::instance()->mixUpColumnsKeyframes(Area_Cells) &&
        !MyParams::instance()->mixUpColumnsKeyframes(Area_Cells).isEmpty()) {
      QString questionMsg =
          tr("Deleting Current Mix-up Columns Keyframes of the Cells Area and "
             "Sharing Keyframes of the Actions Area. Are You Sure?");

      QMessageBox::StandardButton ret =
          QMessageBox::question(this, tr("Question"), questionMsg);
      if (ret != QMessageBox::Yes) {
        m_shareKeysCheckbox->setChecked(false);
        return;
      }
    }

    // キーフレーム情報を一本化
    MyParams::instance()->unifyOrSeparateMixupColumnsKeyframes(true);
    // tableのArea値をUnspecifiedにする
    dynamic_cast<MixupTable*>(m_tableStack->widget(0))
        ->setArea(Area_Unspecified);
    // タブバー隠す
    m_tableTab->hide();
    // スタックのIndexを0にする
    m_tableStack->setCurrentIndex(0);
    dynamic_cast<MixupTable*>(m_tableStack->widget(0))->updateTable();
  } else {
    // OFFにするとき
    //  キーフレーム情報を分岐
    MyParams::instance()->unifyOrSeparateMixupColumnsKeyframes(false);
    // tableのArea値をActions/Cellsにする
    dynamic_cast<MixupTable*>(m_tableStack->widget(0))->setArea(Area_Actions);
    dynamic_cast<MixupTable*>(m_tableStack->widget(1))->setArea(Area_Cells);
    // タブバー表示
    m_tableTab->show();
    dynamic_cast<MixupTable*>(m_tableStack->widget(0))->updateTable();
    dynamic_cast<MixupTable*>(m_tableStack->widget(1))->updateTable();
  }
  MyParams::instance()->notifyTemplateSwitched();
  MyParams::instance()->setFormatDirty(true);
}