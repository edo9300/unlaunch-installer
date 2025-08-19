#ifndef UNLAUNCH_H
#define UNLAUNCH_H
#include <string_view>
#include <span>

#include "consoleInfo.h"

typedef enum UNLAUNCH_VERSION {
	v2_0,
	INVALID,
} UNLAUNCH_VERSION;

static constexpr auto MAX_GIF_SIZE = 0x3C70;

const char* getUnlaunchVersionString(UNLAUNCH_VERSION);

bool uninstallUnlaunch(const consoleInfo& info, bool removeHNAABackup);
bool installUnlaunch(const consoleInfo& info, bool disableAllPatches, const char* splashSoundBinaryPatchPath, std::span<uint8_t> customBackground);

UNLAUNCH_VERSION loadUnlaunchInstaller(std::string_view path);

#endif
