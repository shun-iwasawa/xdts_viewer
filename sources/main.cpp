#include "mywindow.h"
#include "myparams.h"
#include "pathutils.h"
#include "instancemanager.h"

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

  // Single-instance / multi-window coordination. Use QApplication::arguments()
  // (not raw argv) so non-ASCII paths survive on Windows. A Forwarded process
  // has already handed its request to the running mothership and must exit
  // without doing any of the setup below; a Mothership process never shows a
  // window and just serves IPC requests until every worker window has closed.
  // 単一起動・マルチウィンドウの調整。非ASCIIパスがWindowsで壊れないよう、
  // 生のargvではなくQApplication::arguments()を使う。Forwarded(転送済み)の
  // 場合は既に稼働中の母艦へ要求を渡し終えているため、以下の初期化を一切
  // 行わずに終了する。Mothership(母艦)の場合はウィンドウを表示せず、全ての
  // ワーカーウィンドウが閉じるまでIPC要求を処理し続ける。
  QString initialPath;
  InstanceManager::Role role =
      InstanceManager::instance()->bootstrap(a.arguments(), initialPath);
  if (role == InstanceManager::Role::Forwarded) return 0;
  if (role == InstanceManager::Role::Mothership) return a.exec();

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
  MyParams::instance()->initStamps();

#ifndef __MACOS__
  QWindowsWindowFunctions::setWinTabEnabled(true);
#endif

  if (!initialPath.isEmpty())
    MyParams::instance()->setCurrentXdtsPath(initialPath);
  MyWindow w;

  // Worker windows receive reload/activate pushes from the mothership when
  // another launch requests the file already open in this window.
  // ワーカーウィンドウは、別の起動がこのウィンドウで既に開いているファイル
  // を要求した際、母艦からの再読み込み・前面化の指示を受け取る。
  if (role == InstanceManager::Role::Worker) {
    QObject::connect(InstanceManager::instance(),
                     &InstanceManager::reloadAndActivateRequested, &w,
                     &MyWindow::reloadAndActivate);
  }

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