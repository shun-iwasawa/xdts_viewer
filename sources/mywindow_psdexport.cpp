
#ifdef WIN32
#include "mywindow.h"
#include "xsheetpreviewarea.h"

#include <algorithm>
#include <QFileInfo>
#include <QMessageBox>
#include <QDir>

#include <QList>
#include <QPair>
#include <QImage>
#include <QDesktopServices>
#include <QUrl>

// the main include that always needs to be included in every translation unit
// that uses the PSD library
#include "Psd.h"

// for convenience reasons, we directly include the platform header from the PSD
// library. we could have just included <Windows.h> as well, but that is
// unnecessarily big, and triggers lots of warnings.
#include "PsdPlatform.h"

// in the sample, we use the provided malloc allocator for all memory
// allocations. likewise, we also use the provided native file interface. in
// your code, feel free to use whatever allocator you have lying around.
#include "PsdMallocAllocator.h"
#include "PsdNativeFile.h"

#include "PsdDocument.h"
#include "PsdColorMode.h"
#include "PsdLayer.h"
#include "PsdChannel.h"
#include "PsdChannelType.h"
#include "PsdLayerMask.h"
#include "PsdVectorMask.h"
#include "PsdLayerMaskSection.h"
#include "PsdImageDataSection.h"
#include "PsdImageResourcesSection.h"
#include "PsdParseDocument.h"
#include "PsdParseLayerMaskSection.h"
#include "PsdParseImageDataSection.h"
#include "PsdParseImageResourcesSection.h"
#include "PsdLayerCanvasCopy.h"
#include "PsdInterleave.h"
#include "PsdPlanarImage.h"
#include "PsdExport.h"
#include "PsdExportDocument.h"

using namespace psd;

namespace {

void doSavePsd(QString fileName, QList<QPair<QString, QImage>> layers) {
  QSize imageSize = layers[0].second.size();

  unsigned char* channelBuffer[4];
  for (int c = 0; c < 4; c++)
    channelBuffer[c] =
        new unsigned char[imageSize.width() * imageSize.height()];

  const std::wstring dstPath = fileName.toStdWString();

  MallocAllocator allocator;
  NativeFile file(&allocator);

  // try opening the file. if it fails, bail out.
  if (!file.OpenWrite(dstPath.c_str())) {
    std::cout << "Cannot open file." << std::endl;
    return;
  }

  // write an RGB PSD file, 8-bit
  ExportDocument* document =
      CreateExportDocument(&allocator, imageSize.width(), imageSize.height(),
                           8u, exportColorMode::RGB);

  // metadata can be added as simple key-value pairs.
  // when loading the document, they will be contained in XMP metadata such as
  // e.g. <xmp:MyAttribute>MyValue</xmp:MyAttribute>
  // AddMetaData(document, &allocator, "MyAttribute", "MyValue");

  for (auto layer : layers) {
    // when adding a layer to the document, you first need to get a new index
    // into the layer table. with a valid index, layers can be updated in
    // parallel, in any order. this also allows you to only update the layer
    // data that has changed, which is crucial when working with large data
    // sets.
    const unsigned int layerId =
        AddLayer(document, &allocator, layer.first.toStdString().c_str());

    unsigned char* b_p = channelBuffer[0];
    unsigned char* g_p = channelBuffer[1];
    unsigned char* r_p = channelBuffer[2];
    unsigned char* a_p = channelBuffer[3];
    for (int y = 0; y < layer.second.height(); y++) {
      unsigned char* c_p = layer.second.scanLine(y);
      for (int x = 0; x < layer.second.width();
           x++, r_p++, g_p++, b_p++, a_p++) {
        *b_p = *(c_p++);
        *g_p = *(c_p++);
        *r_p = *(c_p++);
        *a_p = *(c_p++);
      }
    }

    // note that each layer has its own compression type. it is perfectly
    // legal to compress different channels of different layers with different
    // settings. RAW is pretty much just a raw data dump. fastest to write,
    // but large. RLE stores run-length encoded data which can be good for
    // 8-bit channels, but not so much for 16-bit or 32-bit data. ZIP is a
    // good compromise between speed and size. ZIP_WITH_PREDICTION first delta
    // encodes the data, and then zips it. slowest to write, but also smallest
    // in size for most images.
    UpdateLayer(document, &allocator, layerId, exportChannel::RED, 0, 0,
                imageSize.width(), imageSize.height(), channelBuffer[2],
                compressionType::RLE);
    UpdateLayer(document, &allocator, layerId, exportChannel::GREEN, 0, 0,
                imageSize.width(), imageSize.height(), channelBuffer[1],
                compressionType::RLE);
    UpdateLayer(document, &allocator, layerId, exportChannel::BLUE, 0, 0,
                imageSize.width(), imageSize.height(), channelBuffer[0],
                compressionType::RLE);
    // note that transparency information is always supported, regardless of
    // the export color mode. it is saved as true transparency, and not as
    // separate alpha channel.
    UpdateLayer(document, &allocator, layerId, exportChannel::ALPHA, 0, 0,
                imageSize.width(), imageSize.height(), channelBuffer[3],
                compressionType::RLE);
  }
  WriteDocument(document, &allocator, &file);

  DestroyExportDocument(document, &allocator);
  file.Close();

  for (int c = 0; c < 4; c++) delete[] channelBuffer[c];
}

}  // namespace

void MyWindow::onExportPSD(QString fileName) {
  if (QFileInfo(fileName).exists()) {
    QString msgStr =
        tr("The file %1 already exists.\nDo you want to overwrite them?")
            .arg(fileName);
    int ret = QMessageBox::question(
        this, tr("Question"), msgStr,
        QMessageBox::StandardButtons(QMessageBox::Ok | QMessageBox::Cancel),
        QMessageBox::Ok);
    if (ret != QMessageBox::Ok) return;
  }

  // ‚±‚±‚©‚ç‘‚«o‚µ
  QList<QPair<QString, QImage>> layers;

  XSheetPDFTemplate* tmpl = MyParams::instance()->currentTemplate();

  MyWindow::setInfo(true);
  bool hasBackside = !MyParams::instance()->backsideImgPath().isEmpty();
  int framePageCount =
      std::max(tmpl->framePageCount(),
               MyParams::instance()->getScannedSheetPageAmount());
  int parallelPageCount = tmpl->parallelPageCount();

  QSize pixelSize = tmpl->getPixelSize();

  int currentPage = (hasBackside) ? 0 : 1;
  for (int fpage = (hasBackside) ? -1 : 0; fpage < framePageCount; fpage++) {
    for (int ppage = 0; ppage < parallelPageCount; ppage++, currentPage++) {
      QString pageStr = QString::number(fpage + 1);
      if (parallelPageCount > 1 && fpage >= 0) pageStr += QChar('A' + ppage);

      if (fpage == -1) {
        QPixmap backPm(pixelSize);
        backPm.fill(Qt::transparent);
        QPainter painter(&backPm);
        painter.drawPixmap(0, 0, MyParams::instance()->backsidePixmap(true));
        painter.end();

        layers.append({pageStr + "_template", backPm.toImage()});
      } else {
        QPixmap tmplPm = tmpl->initializePreview();
        QPainter painter(&tmplPm);
        painter.setRenderHint(QPainter::Antialiasing);
        tmpl->drawXsheetTemplate(painter, fpage, true);
        layers.append({pageStr + "_template", tmplPm.toImage()});
        painter.end();

        QPixmap contentsPm(tmpl->getPixelSize());
        contentsPm.fill(Qt::transparent);
        painter.begin(&contentsPm);
        painter.setRenderHint(QPainter::Antialiasing);
        tmpl->drawXsheetContents(painter, fpage, ppage, true);
        layers.append({pageStr + "_numbers", contentsPm.toImage()});
        painter.end();
      }
      QPixmap memoPm(pixelSize);
      memoPm.fill(Qt::transparent);
      QPainter painter(&memoPm);
      painter.drawImage(0, 0, MyParams::instance()->scribbleImage(currentPage));
      painter.end();
      layers.append({pageStr + "_memo", memoPm.toImage()});

      if (fpage == -1) {
        currentPage++;
        break;
      }
    }
  }

  doSavePsd(fileName, layers);

  std::vector<QString> buttons = {tr("OK"), tr("Open containing folder")};
  int ret = QMessageBox::information(this, tr("Export Images"),
                                     tr("Xsheet images are exported properly."),
                                     buttons[0], buttons[1]);

  if (ret == 1) {
    QString dirPath = QFileInfo(fileName).absoluteDir().path();
    QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));
  }
  // info–ß‚µ
  MyWindow::setInfo();
}

#endif