#include "pathutils.h"

#include <QDir>
#include <QFileInfo>
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

// Falls back to a cleaned absolute path when the file doesn't exist yet, and
// folds case on Windows (NTFS is case-insensitive).
// ファイルがまだ存在しない場合は絶対パスをクリーンアップした値にフォールバックし、
// Windows では大文字小文字を無視する（NTFSは大文字小文字を区別しないため）。
QString canonicalizePath(const QString& path) {
  if (path.isEmpty()) return QString();

  QString canonical = QFileInfo(path).canonicalFilePath();
  if (canonical.isEmpty())
    canonical = QDir::cleanPath(QFileInfo(path).absoluteFilePath());

#ifndef __MACOS__
  canonical = canonical.toLower();
#endif

  return canonical;
}
}  // namespace PathUtils
