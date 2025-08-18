#include "mywindow.h"
#include "myparams.h"
#include "pathutils.h"

#include <iostream>

// Qt includes
#include <QApplication>
#include <QTranslator>
#include <QDir>
#include <QScreen>

#ifndef __MACOS__
#include <QtPlatformHeaders/QWindowsWindowFunctions>
#endif
//-----------------------------------------------------------------------------

int main(int argc, char* argv[]) {
  QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
  QApplication a(argc, argv);
  a.setApplicationName("XDTS Viewer");
  a.setApplicationVersion("1.2.0");

  // Compress tablet events with application attributes instead of implementing
  // the delay-timer by ourselves
  a.setAttribute(Qt::AA_CompressHighFrequencyEvents);
  a.setAttribute(Qt::AA_CompressTabletEvents);

  MyParams::instance()->initialize();

  QTranslator tra;
  QString lang = MyParams::instance()->language();
  if (lang != "en") {
    QString qmFileName = QString("XDTS_Viewer_%1").arg(lang);
    if (QDir(PathUtils::getTranslationFolderPath())
            .exists(qmFileName + ".qm")) {
      tra.load(qmFileName, PathUtils::getTranslationFolderPath());
      a.installTranslator(&tra);
    }
  }

#ifndef __MACOS__
  QWindowsWindowFunctions::setWinTabEnabled(true);
#endif

  if (argc > 1) {
    QString initialPath = QString::fromLocal8Bit(argv[1]);
    MyParams::instance()->setCurrentXdtsPath(initialPath);
  }
  MyWindow w;

  QRect geometry = MyParams::instance()->loadWindowGeometry();
  if (!geometry.isNull()) {
    for (auto screen : a.screens()) {
      if (screen->availableGeometry().contains(geometry.center())) {
        w.setGeometry(geometry);
        break;
      }
    }
  }

  w.show();
  w.initialize();
  int ret = a.exec();
  return ret;
}