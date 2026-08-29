#ifndef UNLAUNCH_H
#define UNLAUNCH_H
#include <string_view>
#include <span>

#include "consoleInfo.h"

typedef enum UNLAUNCH_VERSION {
	v2_0,
	CUSTOM,
	INVALID,
} UNLAUNCH_VERSION;

struct unlaunchInstallOptions {
	bool noLauncherPatches;
	bool disableSound;
	bool disableHealthAndSafety;
	std::span<uint8_t> customBackground;
};

static constexpr auto MAX_GIF_SIZE = 0x3C70;

const char* getUnlaunchVersionString(UNLAUNCH_VERSION);

bool uninstallUnlaunch(const consoleInfo& info, bool removeHNAABackup);
bool installUnlaunch(const consoleInfo& info, const unlaunchInstallOptions& options);

UNLAUNCH_VERSION loadUnlaunchInstaller(std::string_view path);
UNLAUNCH_VERSION loadUnlaunchLikeHomebrew(std::string_view path);

#endif
