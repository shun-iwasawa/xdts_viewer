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

// Always a pure string normalization (no OS round-trip via
// QFileInfo::canonicalFilePath()), so two processes calling this for the
// same path get identical results regardless of timing or whether the file
// exists yet in either process (this matters for InstanceManager, where the
// mothership and a worker process each canonicalize the same path
// independently). Folds case on Windows (NTFS is case-insensitive).
// QFileInfo::canonicalFilePath()によるOSへの問い合わせは行わない、純粋な文字列
// 正規化のみを行う。これにより、別々のプロセスが同じパスに対してこの関数を
// 呼んでも、タイミングやファイルの存在有無に関わらず同一の結果が得られる
// （InstanceManagerで母艦とワーカーが同じパスをそれぞれ独立に正規化する際に
// 重要）。Windows では大文字小文字を無視する（NTFSは大文字小文字を区別し
// ないため）。
QString canonicalizePath(const QString& path) {
  if (path.isEmpty()) return QString();

  QString canonical = QDir::cleanPath(QFileInfo(path).absoluteFilePath());

#ifndef __MACOS__
  canonical = canonical.toLower();
#endif

  return canonical;
}
}  // namespace PathUtils
