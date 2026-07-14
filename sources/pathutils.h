#pragma once
#ifndef PATHUTILS_H
#define PATHUTILS_H

#include <QString>

namespace PathUtils {
QString configDirPath();
QString getUserSettingsPath();
QString getPresetDirPath();
QString getDefaultFormatSettingsPath();
QString getResourceDirPath();
QString getProjectRoot();
QString getLicenseFolderPath();
QString getTranslationFolderPath();

// Normalizes a path for cross-process "is this file already open"
// comparisons (used by InstanceManager).
// 別プロセス間で「同じファイルが既に開いているか」を比較するためにパスを正規化する。
QString canonicalizePath(const QString& path);
}  // namespace PathUtils
#endif