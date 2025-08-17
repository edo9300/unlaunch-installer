#ifndef UNLAUNCH_H
#define UNLAUNCH_H
#include <string_view>

#include "consoleInfo.h"

typedef enum UNLAUNCH_VERSION {
	v2_0,
	INVALID,
} UNLAUNCH_VERSION;

const char* getUnlaunchVersionString(UNLAUNCH_VERSION);

bool uninstallUnlaunch(const consoleInfo& info, bool removeHNAABackup);
bool installUnlaunch(const consoleInfo& info, bool disableAllPatches, const char* splashSoundBinaryPatchPath, const char* customBackgroundPath);

UNLAUNCH_VERSION loadUnlaunchInstaller(std::string_view path);

#endif
