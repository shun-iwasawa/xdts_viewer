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
}  // namespace PathUtils
#endif