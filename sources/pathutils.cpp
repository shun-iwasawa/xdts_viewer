#include "pathutils.h"

#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>

namespace PathUtils {
QString configDirPath() {
  return QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
}

QString getUserSettingsPath() {
#ifdef __MACOS__
  return configDirPath() + "/xdts_viewer_user_settings.ini";
#endif
#ifndef __MACOS__
  return configDirPath() + "/usersettings.ini";
#endif
}

QString getResourceDirPath() {
  return QCoreApplication::applicationDirPath() + "/xdts_viewer_resources";
}

QString getPresetDirPath() { return getResourceDirPath() + "/xsheettemplates"; }

QString getDefaultFormatSettingsPath() {
  return getResourceDirPath() + "/defaultconfig.ini";
}

QString getProjectRoot() {
  return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

QString getLicenseFolderPath() { return getResourceDirPath() + "/LICENSE"; }

QString getTranslationFolderPath() { return getResourceDirPath() + "/loc"; }
}  // namespace PathUtils
