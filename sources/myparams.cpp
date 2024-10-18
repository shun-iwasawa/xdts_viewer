#include "myparams.h"

#include "xsheetpreviewarea.h"
#include "tool.h"
#include "pathutils.h"

#include <QCoreApplication>
#include <QFileInfo>
#include <QSettings>
#include <QSettings>
#include <QDir>
#include <QImageReader>
#include <QFileDialog>
#include <QMessageBox>

namespace {

const QString configFileName         = "config.ini";
const QStringList exportAreaNameList = {"Actions", "Cells"};

QString pageNumber2Name(int page, int parallelPageCount) {
  if (parallelPageCount == 1 || page == 0) return QString::number(page);

  int fPage = (page - 1) / parallelPageCount;
  int pPage = (page - 1) % parallelPageCount;

  char suffix = 'A' + pPage;

  return QString::number(fPage) + QString(suffix);
}

}  // namespace

//-----------------------------------------------------------------------------

MyParams::MyParams()
    : m_templateName("B4 size, 6 seconds sheet")
    , m_exportArea(Area_Cells)
    , m_logoPath()
    , m_skippedLevelNames()
    , m_language("ja")
    , m_lineColor(156, 200, 98)
#ifdef MACOSX
    , m_contentsFont("Helvetica Neue")
    , m_templateFont("Helvetica Neue")
#else
    , m_contentsFont("BIZ UDPGothic")
    , m_templateFont("MS Gothic")
#endif
    , m_continuousLineMode(Line_MoreThan3s)
    , m_serialFrameNumber(true)
    , m_levelNameOnBottom(false)
    , m_capitalizeFirstLetter(true)
    , m_currentTmpl(nullptr)
    , m_currentToolId(Tool_Brush)
    , m_undoStack(new QUndoStack())
    , m_isBusy(false)
    , m_isFormatDirty(false)
    , m_userStampFolderPath()
    , m_approvalName()
    , m_expandColumns(true)
    , m_mixUpColumns(true)
    , m_backsideImgPath()
    , m_backsideImgPathWithDenpyo()
    , m_isScannedGengaSheet(false)
    , m_dougaColumnOffset(0)
    , m_scannedSheetPageAmount(1)
    , m_startOverlapFrameLength(0)
    , m_endOverlapFrameLength(0)
    , m_currentColor(Qt::black)
    , m_exportLineColor(156, 200, 98)
    , m_exportTemplateFont("MS Gothic")
    , m_exportContentsFont("BIZ UDPGothic") {
  m_scribbleImages.clear();
  m_suffixes = {{Genga, "_genga_ts"},
                {Douga, "_douga_ts"},
                {LO, "_LO_ts"},
                {RoughGen, "_roughgen_ts"}};
}

void MyParams::initialize() {
  QDir presetFolder = PathUtils::getPresetDirPath();
  // std::cout << "folder: " << presetFolder.absolutePath().toStdString() <<
  // std::endl;
  if (presetFolder.exists() && QFileInfo(presetFolder.path()).isDir()) {
    QStringList filters;
    filters << "*.ini";
    presetFolder.setNameFilters(filters);
    presetFolder.setFilter(QDir::Files);
    QStringList entries = (presetFolder.entryList(
        presetFolder.filter() | QDir::NoDotAndDotDot, QDir::Name));
    QStringList pathSet;
    for (auto e : entries) {
      pathSet.append(presetFolder.absoluteFilePath(e));
    }

    for (auto fp : pathSet) {
      QSettings s(fp, QSettings::IniFormat);
      if (!(s.childGroups().contains("XSheetPDFTemplate"))) continue;
      s.beginGroup("XSheetPDFTemplate");
      QString labelStr = s.value("Label", QFileInfo(fp).baseName()).toString();
      registerTemplate(labelStr, fp);
      if (m_templateName.isEmpty()) m_templateName = labelStr;
    }
  }

  loadUserSettingsIfExists();
  // マイスタンプの登録ここでやる
  registerDefaultStamps();
  loadUserStamps();

  QPixmap pm(5, 5);
  pm.fill(Qt::transparent);
  QPainter p(&pm);
  p.setPen(Qt::black);
  p.drawLine(2, 0, 2, 4);
  p.drawLine(0, 2, 4, 2);
  m_brushCursor = QCursor(pm);
}

//-----------------------------------------------------------------------------

MyParams* MyParams::instance() {
  static MyParams _instance;
  return &_instance;
}

MyParams::~MyParams() {
  // saveUserSettings();

  if (m_currentTmpl) delete m_currentTmpl;
  delete m_undoStack;
}

QString MyParams::templatePath() const {
  if (m_templateName.isEmpty()) return QString();
  return m_templatesMap.value(m_templateName, "");
}

void MyParams::setCurrentTemplate(XSheetPDFTemplate* tmpl) {
  if (m_currentTmpl) {
    delete m_currentTmpl;
    m_currentTmpl = nullptr;
  }
  m_currentTmpl = tmpl;
}

void MyParams::setBacksideImgPath(const QString& val) {
  if (m_withDenpyo)
    m_backsideImgPathWithDenpyo = val;
  else
    m_backsideImgPath = val;
  emit(backsideImgPathChanged());
}

QImage MyParams::scribbleImage(int page) {
  QSize templatePixelSize = m_currentTmpl->getPixelSize();
  // mapにあればそれを返す
  if (m_scribbleImages.contains(page)) {
    QImage img = m_scribbleImages.value(page);
    // 画像がテンプレートサイズよりも小さい場合、拡張する
    if (img.size() != templatePixelSize) {
      img = img.copy(0, 0, std::max(img.width(), templatePixelSize.width()),
                     std::max(img.height(), templatePixelSize.height()));
      m_scribbleImages.insert(page, img);
    }
    return img;
  }

  // 無ければファイルをロードする
  QString imgName =
      pageNumber2Name(page, m_currentTmpl->parallelPageCount()) + ".png";

  if (QDir(getImageFolderPath()).exists(imgName)) {
    QImage img;
    img.load(getImageFolderPath() + "/" + imgName);
    if (!img.isNull()) {
      // 画像がテンプレートサイズよりも小さい場合、拡張する
      if (img.size() != templatePixelSize) {
        img = img.copy(0, 0, std::max(img.width(), templatePixelSize.width()),
                       std::max(img.height(), templatePixelSize.height()));
      }
      m_scribbleImages.insert(page, img);
      m_dirtyFlags.insert(page, false);
      return img;
    }
  }

  QImage img(templatePixelSize, QImage::Format_ARGB32_Premultiplied);
  img.fill(Qt::transparent);
  m_scribbleImages.insert(page, img);
  m_dirtyFlags.insert(page, false);
  return img;
}

void MyParams::setScribbleImage(int page, const QImage& image) {
  m_scribbleImages.insert(page, image);
}
void MyParams::resetValues() {
  m_templateName  = "B4 size, 6 seconds sheet";
  m_expandColumns = true;
  if (!isAreaSpecified()) m_exportArea = Area_Cells;
  m_logoPath                  = "";
  m_withDenpyo                = false;
  m_backsideImgPath           = "";
  m_backsideImgPathWithDenpyo = "";
  m_skippedLevelNames         = "";
  m_exportLineColor           = QColor(156, 200, 98);
  m_exportTemplateFont        = "MS Gothic";
  m_exportContentsFont        = "BIZ UDPGothic";
  m_mixUpColumns              = true;
  m_isScannedGengaSheet       = false;
  m_dougaColumnOffset         = 0;
  m_scannedSheetPageAmount    = 1;

  if (QFileInfo(PathUtils::getDefaultFormatSettingsPath()).exists()) {
    QSettings settings(PathUtils::getDefaultFormatSettingsPath(),
                       QSettings::IniFormat);
    settings.beginGroup("FormatSettings");
    m_templateName  = settings.value("TemplateName", m_templateName).toString();
    m_expandColumns = settings.value("ExpandColumns", m_expandColumns).toBool();
    m_mixUpColumns  = settings.value("MixUpColumns", m_mixUpColumns).toBool();
    QString areaName =
        settings.value("ExportArea", exportAreaNameList[m_exportArea])
            .toString();
    if (!isAreaSpecified())
      m_exportArea = (ExportArea)exportAreaNameList.indexOf(areaName);
    m_logoPath   = settings.value("LogoPath", m_logoPath).toString();
    m_withDenpyo = settings.value("WithDenpyo", m_withDenpyo).toBool();
    m_backsideImgPath =
        settings.value("BacksideImgPath", m_backsideImgPath).toString();
    m_backsideImgPathWithDenpyo =
        settings.value("BacksideImgPathWithDenpyo", m_backsideImgPathWithDenpyo)
            .toString();
    m_skippedLevelNames =
        settings.value("SkippedLevelNames", m_skippedLevelNames).toString();
    m_isScannedGengaSheet =
        settings.value("IsScannedGengaSheet", m_isScannedGengaSheet).toBool();
    m_dougaColumnOffset =
        settings.value("DougaColumnOffset", m_dougaColumnOffset).toInt();
    m_scannedSheetPageAmount =
        settings.value("ScannedSheetPageAmount", m_scannedSheetPageAmount)
            .toInt();
    m_startOverlapFrameLength =
        settings.value("StartOverlapFrameLength", m_startOverlapFrameLength)
            .toInt();
    m_endOverlapFrameLength =
        settings.value("EndOverlapFrameLength", m_endOverlapFrameLength)
            .toInt();
    settings.endGroup();
    settings.beginGroup("ExportSettings");
    m_exportLineColor.setNamedColor(
        settings.value("ExportLineColor", m_exportLineColor.name()).toString());
    m_exportTemplateFont =
        settings.value("ExportTemplateFont", m_exportTemplateFont).toString();
    m_exportContentsFont =
        settings.value("ExportContentsFont", m_exportContentsFont).toString();
    settings.endGroup();
  }
  setFormatDirty(false);
  m_scribbleImages.clear();
  m_dirtyFlags.clear();
  m_undoStack->clear();
}

QPixmap MyParams::backsidePixmap(bool forExportImage) {
  QPixmap retPm(m_currentTmpl->getPixelSize());
  retPm.fill(Qt::white);

  if (backsideImgPath().isEmpty()) return retPm;

  QImage img(backsideImgPath());
  if (img.isNull()) return retPm;

  QImage alphaImg = img;
  alphaImg.invertPixels();
  img.setAlphaChannel(alphaImg);
  QImage backsideImg(img.size(), QImage::Format_ARGB32_Premultiplied);
  backsideImg.fill((forExportImage) ? m_exportLineColor : m_lineColor);
  QPainter painter(&backsideImg);
  painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
  painter.drawImage(QPoint(0, 0), img);
  painter.end();

  QPainter p(&retPm);
  p.drawImage(QPoint(), backsideImg);
  p.end();
  return retPm;
}

// ./aaa0001.xdts -> ./aaa0001_xdts
QString MyParams::getImageFolderPath(QString xdtsPath) {
  if (xdtsPath.isEmpty()) xdtsPath = currentXdtsPath();

  if (xdtsPath.isEmpty() || !xdtsPath.endsWith(".xdts")) return QString();

  QFileInfo fi(xdtsPath);
  QString fileName = fi.fileName();

  // _genga_ts, _douga_ts を含む場合は取り除く
  for (int type = (int)Genga; type < (int)WorkFlowTypeCount; type++)
    fileName = fileName.remove(MyParams::instance()->suffix((WorkFlowType)type),
                               Qt::CaseInsensitive);

  QString ret(fi.dir().path() + "/" + fileName);
  ret.chop(5);
  return ret + QString("_xdts");
}

void MyParams::setImageDirty(int page) {
  if (m_dirtyFlags.value(page, false)) return;
  m_dirtyFlags.insert(page, true);
  emit(dirtyStateChanged());
}

void MyParams::setFormatDirty(bool dirty) {
  if (m_isFormatDirty == dirty) return;
  m_isFormatDirty = dirty;
  emit(dirtyStateChanged());
}

bool MyParams::loadFormatSettingsIfExists() {
  if (!QDir(getImageFolderPath()).exists(configFileName)) return false;

  QSettings settings(getImageFolderPath() + "/" + configFileName,
                     QSettings::IniFormat);
  settings.beginGroup("FormatSettings");
  m_templateName  = settings.value("TemplateName", m_templateName).toString();
  m_expandColumns = settings.value("ExpandColumns", m_expandColumns).toBool();
  m_mixUpColumns  = settings.value("MixUpColumns", true).toBool();
  m_logoPath      = settings.value("LogoPath", m_logoPath).toString();
  m_withDenpyo    = settings.value("WithDenpyo", m_withDenpyo).toBool();
  m_backsideImgPath =
      settings.value("BacksideImgPath", m_backsideImgPath).toString();
  m_backsideImgPathWithDenpyo =
      settings.value("BacksideImgPathWithDenpyo", m_backsideImgPathWithDenpyo)
          .toString();
  m_skippedLevelNames =
      settings.value("SkippedLevelNames", m_skippedLevelNames).toString();
  m_isScannedGengaSheet = settings.value("IsScannedGengaSheet", false).toBool();
  m_dougaColumnOffset   = settings.value("DougaColumnOffset", 0).toInt();
  m_scannedSheetPageAmount =
      settings.value("ScannedSheetPageAmount", 1).toInt();
  m_startOverlapFrameLength =
      settings.value("StartOverlapFrameLength", 0).toInt();
  m_endOverlapFrameLength = settings.value("EndOverlapFrameLength", 0).toInt();

  QString areaName =
      settings.value("ExportArea", exportAreaNameList[m_exportArea]).toString();
  m_exportArea = (ExportArea)exportAreaNameList.indexOf(areaName);
  // 原画/動画シートの場合は表示エリア決め打ち
  if (m_currentXdtsPaths.size() == 1 &&
      !m_currentXdtsPaths.contains(Area_Unspecified))
    m_exportArea = m_currentXdtsPaths.firstKey();
  settings.endGroup();

  setFormatDirty(false);
  return true;
}

void MyParams::saveFormatSettings() {
  QSettings settings(getImageFolderPath() + "/" + configFileName,
                     QSettings::IniFormat);
  settings.setValue("Version", qApp->applicationVersion());
  settings.beginGroup("FormatSettings");
  settings.setValue("TemplateName", m_templateName);
  settings.setValue("ExpandColumns", m_expandColumns);
  settings.setValue("MixUpColumns", m_mixUpColumns);
  settings.setValue("ExportArea", exportAreaNameList[m_exportArea]);
  settings.setValue("LogoPath", m_logoPath);
  settings.setValue("WithDenpyo", m_withDenpyo);
  settings.setValue("BacksideImgPath", m_backsideImgPath);
  settings.setValue("BacksideImgPathWithDenpyo", m_backsideImgPathWithDenpyo);
  settings.setValue("SkippedLevelNames", m_skippedLevelNames);
  settings.setValue("IsScannedGengaSheet", m_isScannedGengaSheet);
  settings.setValue("DougaColumnOffset", m_dougaColumnOffset);
  settings.setValue("ScannedSheetPageAmount", m_scannedSheetPageAmount);
  settings.setValue("StartOverlapFrameLength", m_startOverlapFrameLength);
  settings.setValue("EndOverlapFrameLength", m_endOverlapFrameLength);
  settings.endGroup();

  setFormatDirty(false);
}

bool MyParams::loadUserSettingsIfExists() {
  if (!QFileInfo(PathUtils::getUserSettingsPath()).exists()) return false;
  QSettings settings(PathUtils::getUserSettingsPath(), QSettings::IniFormat);
  settings.beginGroup("UserSettings");
  m_language = settings.value("Language", m_language).toString();
  m_lineColor.setNamedColor(
      settings.value("LineColor", m_lineColor.name()).toString());
  // m_templateFont = settings.value("TemplateFont", m_templateFont).toString();
  // m_contentsFont = settings.value("ContentsFont", m_contentsFont).toString();
  m_continuousLineMode =
      (ContinuousLineMode)settings
          .value("ContinuousLineMode", (int)m_continuousLineMode)
          .toInt();
  m_serialFrameNumber =
      settings.value("SerialFrameNumber", m_serialFrameNumber).toBool();
  m_levelNameOnBottom =
      settings.value("LevelNameOnBottom", m_levelNameOnBottom).toBool();
  m_capitalizeFirstLetter =
      settings.value("CapitalizeFirstLetter", m_capitalizeFirstLetter).toBool();
  m_userStampFolderPath =
      settings.value("UserStampFolderPath", m_userStampFolderPath).toString();
  m_approvalName = settings.value("ApprovalName", m_approvalName).toString();
  settings.endGroup();
  return true;
}

void MyParams::registerDefaultStamps() {
  StampTool* stampTool = dynamic_cast<StampTool*>(getTool(Tool_Stamp));
  // スタンプ画像の登録
  QPixmap circlePm(55, 55);
  circlePm.fill(Qt::transparent);
  QPainter p(&circlePm);
  p.setRenderHints(QPainter::Antialiasing);
  p.setPen(QPen(Qt::black, 2. / (double)circlePm.width()));
  p.scale(circlePm.width(), circlePm.height());
  p.drawEllipse(QPointF(0.5, 0.5), 0.4, 0.4);
  p.end();
  stampTool->registerStamp(tr("Circle"), circlePm);

  QPixmap trianglePm(60, 60);
  trianglePm.fill(Qt::transparent);
  p.begin(&trianglePm);
  p.setRenderHints(QPainter::Antialiasing);
  p.setPen(QPen(Qt::black, 2. / (double)trianglePm.width()));
  p.scale(trianglePm.width(), trianglePm.height());
  QPointF points[4] = {QPointF(0.5, 0.02), QPointF(0.1, 0.74),
                       QPointF(0.9, 0.74), QPointF(0.5, 0.02)};
  p.drawPolyline(points, 4);
  p.end();
  stampTool->registerStamp(tr("Triangle"), trianglePm);

  if (MyParams::instance()->approvalName().isEmpty()) return;
  QPixmap okStampPm(120, 120);
  okStampPm.fill(Qt::transparent);
  p.begin(&okStampPm);
  p.setRenderHints(QPainter::Antialiasing);
  p.setPen(QPen(QColor(255, 62, 30), 3.));
  p.drawEllipse(1, 1, 118, 118);
  p.drawLine(1, 60, 119, 60);
  p.setFont(QFont("Meiryo", 12));
  p.drawText(QRectF(0, 2, 120, 25), Qt::AlignCenter,
             QString::number(QDate::currentDate().year()));
  p.setFont(QFont("Meiryo", 25));
  p.drawText(QRectF(0, 23, 120, 37), Qt::AlignCenter,
             QDate::currentDate().toString("MM/dd"));
  p.setFont(QFont(QString::fromLocal8Bit("UD デジタル 教科書体 NK-B"), 27));
  QString name = MyParams::instance()->approvalName();
  QFontMetrics fm(p.font());
  int w         = fm.horizontalAdvance(name);
  double xRatio = 1.;
  if (w > 80) {
    xRatio = 80. / (double)w;
    p.scale(xRatio, 1.);
  }
  p.drawText(QRectF(20 / xRatio, 70, 80 / xRatio, 34), Qt::AlignCenter, name);
  p.end();
  stampTool->registerStamp(tr("Approve"), okStampPm);
}

void MyParams::loadUserStamps() {
  if (m_userStampFolderPath.isEmpty()) return;
  QDir userStampFolder = m_userStampFolderPath;
  if (!userStampFolder.exists()) return;
  StampTool* stampTool = dynamic_cast<StampTool*>(getTool(Tool_Stamp));

  QStringList filters;
  for (QByteArray& format : QImageReader::supportedImageFormats()) {
    filters << "*." + format;
    // std::cout << QString(format).toStdString() << std::endl;
  }
  userStampFolder.setNameFilters(filters);
  userStampFolder.setFilter(QDir::Files);
  QStringList entries = userStampFolder.entryList(
      userStampFolder.filter() | QDir::NoDotAndDotDot, QDir::Name);

  for (auto e : entries) {
    QPixmap pm(userStampFolder.absoluteFilePath(e));
    stampTool->registerStamp(e, pm);
  }
}

void MyParams::saveUserSettings() {
  QSettings settings(PathUtils::getUserSettingsPath(), QSettings::IniFormat);
  settings.beginGroup("UserSettings");
  settings.setValue("Language", m_language);
  settings.setValue("LineColor", m_lineColor.name());
  // settings.setValue("TemplateFont", m_templateFont);
  // settings.setValue("ContentsFont", m_contentsFont);
  settings.setValue("ContinuousLineMode", (int)m_continuousLineMode);
  settings.setValue("SerialFrameNumber", m_serialFrameNumber);
  settings.setValue("LevelNameOnBottom", m_levelNameOnBottom);
  settings.setValue("CapitalizeFirstLetter", m_capitalizeFirstLetter);
  settings.setValue("LastOpenedXdtsPath", currentXdtsPath());
  settings.setValue("UserStampFolderPath", m_userStampFolderPath);
  settings.setValue("ApprovalName", m_approvalName);
  settings.endGroup();
}

void MyParams::setCurrentTool(ToolId id) {
  if (m_currentToolId == id) return;
  m_currentToolId = id;
  currentTool()->onActivate();
  emit toolSwitched();
}

void MyParams::setToolView(XsheetPdfPreviewPane* view) {
  for (auto tool : m_tools.values()) {
    tool->setView(view);
  }
}

bool MyParams::somethingIsDirty() {
  if (m_isFormatDirty) return true;

  for (auto flag : m_dirtyFlags.values())
    if (flag) return true;

  return false;
}

void MyParams::saveChanges() {
  // フォルダ作成
  QString imageFolderPath = getImageFolderPath();
  if (!QDir(imageFolderPath).exists()) {
    QDir folderPath(imageFolderPath);
    folderPath.cdUp();
    folderPath.mkdir(QDir(imageFolderPath).dirName());
    // save format settings even if it is unchanged from the default
    m_isFormatDirty = true;
  }
  // dirtyな画像を保存
  QSize templatePixelSize = m_currentTmpl->getPixelSize();
  for (int page : m_scribbleImages.keys()) {
    if (m_dirtyFlags.value(page, false)) {
      QString imgName =
          pageNumber2Name(page, m_currentTmpl->parallelPageCount()) + ".png";
      // 保存時は現在のテンプレートサイズに切り抜いて保存する
      QImage img =
          m_scribbleImages.value(page).copy(QRect(QPoint(), templatePixelSize));
      img.save(QDir(imageFolderPath).absoluteFilePath(imgName));
      m_dirtyFlags.insert(page, false);
    }
  }

  if (m_isFormatDirty) saveFormatSettings();

  emit(dirtyStateChanged());
}

// return false when canceled
bool MyParams::saveUntitled() {
  QString fileName = QFileDialog::getSaveFileName(
      nullptr, tr("Save Empty XDTS File"),
      PathUtils::getProjectRoot() + "/Untitled.xdts",
      tr("XDTS Files (*.xdts)"));

  if (!fileName.endsWith(".xdts")) fileName.append(".xdts");

  QString imgFolderPath = getImageFolderPath(fileName);
  if (QFileInfo(fileName).exists() || QDir(imgFolderPath).exists()) {
    QString msgStr;
    if (QFileInfo(fileName).exists())
      msgStr += tr("The file %1 already exists.\n").arg(fileName);
    if (QDir(imgFolderPath).exists())
      msgStr += tr("The folder %1 already exists.\n").arg(imgFolderPath);
    msgStr += tr("Do you want to overwrite it?").arg(imgFolderPath);

    int ret = QMessageBox::question(
        nullptr, tr("Question"), msgStr,
        QMessageBox::StandardButtons(QMessageBox::Ok | QMessageBox::Cancel),
        QMessageBox::Ok);
    if (ret != QMessageBox::Ok) return false;
  }

  QString emptyXdtsContents =
      "exchangeDigitalTimeSheet Save Data\n"
      "{\n"
      "    \"timeTables\": [\n"
      "        {\n"
      "            \"duration\": 144,\n"
      "            \"name\" : \"\",\n"
      "            \"timeTableHeaders\" : [\n"
      "                {\n"
      "                    \"fieldId\": 0,\n"
      "                    \"names\" : [\n"
      "                    ]\n"
      "                }\n"
      "            ]\n"
      "        }\n"
      "    ],\n"
      "    \"version\": 5\n"
      "}\n";

  QFile file(fileName);
  if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    QTextStream stream(&file);
    stream << emptyXdtsContents;
    file.close();
  }
  setCurrentXdtsPath(fileName);

  return true;
}

QString MyParams::currentXdtsPath(ExportArea id) {
  if (m_currentXdtsPaths.isEmpty()) return QString();
  if (m_currentXdtsPaths.size() == 1) return m_currentXdtsPaths.begin().value();
  if (!m_currentXdtsPaths.contains(id))
    return m_currentXdtsPaths.begin().value();
  return m_currentXdtsPaths.value(id);
}

// 原画動画のペアを同時に登録
void MyParams::setCurrentXdtsPath(const QString& path) {
  MyParams* p = MyParams::instance();
  m_currentXdtsPaths.clear();
  QString fileName = QFileInfo(path).fileName();
  if (fileName.contains(p->suffix(Genga))) {
    m_currentXdtsPaths.insert(Area_Actions, path);
    fileName = fileName.replace(p->suffix(Genga), p->suffix(Douga),
                                Qt::CaseInsensitive);
    if (QFileInfo(path).dir().exists(fileName)) {
      QString dougaPath = QFileInfo(path).dir().filePath(fileName);
      m_currentXdtsPaths.insert(Area_Cells, dougaPath);
    }
    m_exportArea = Area_Actions;
  } else if (fileName.contains(p->suffix(Douga))) {
    m_currentXdtsPaths.insert(Area_Cells, path);
    fileName = fileName.replace(p->suffix(Douga), p->suffix(Genga),
                                Qt::CaseInsensitive);
    if (QFileInfo(path).dir().exists(fileName)) {
      QString gengaPath = QFileInfo(path).dir().filePath(fileName);
      m_currentXdtsPaths.insert(Area_Actions, gengaPath);
    }
    m_exportArea = Area_Cells;
  }
  // LO と ラフ原 はアクション欄固定
  else if (fileName.contains(p->suffix(LO)) ||
           fileName.contains(p->suffix(RoughGen))) {
    m_currentXdtsPaths.insert(Area_Actions, path);
    m_exportArea = Area_Actions;
  } else
    m_currentXdtsPaths.insert(Area_Unspecified, path);
}

bool MyParams::isAreaSpecified() {
  return !m_currentXdtsPaths.contains(Area_Unspecified);
}

QString MyParams::logoPath(bool asIs) const {
  if (asIs || !QDir::isRelativePath(m_logoPath)) return m_logoPath;
  return QDir(PathUtils::getResourceDirPath()).absoluteFilePath(m_logoPath);
}

QString MyParams::backsideImgPath(bool asIs) const {
  QString retPath =
      (m_withDenpyo) ? m_backsideImgPathWithDenpyo : m_backsideImgPath;

  if (asIs || !QDir::isRelativePath(retPath) || retPath.isEmpty())
    return retPath;
  return QDir(PathUtils::getResourceDirPath()).absoluteFilePath(retPath);
}
