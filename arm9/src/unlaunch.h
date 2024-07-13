#ifndef UNLAUNCH_H
#define UNLAUNCH_H
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum UNLAUNCH_VERSION {
	v1_8,
	v1_9,
	v2_0,
	INVALID,
} UNLAUNCH_VERSION;

const char* getUnlaunchVersionString(UNLAUNCH_VERSION);

bool uninstallUnlaunch(bool notProto, bool hasHNAABackup, const char* retailLauncherTmdPath, const char* retailLauncherPath, bool removeHNAABackup);
bool installUnlaunch(bool retailConsole, const char* retailLauncherTmdPath, const char* retailLauncherPath, bool disableAllPatches, const char* splashSoundBinaryPatchPath, const char* customBackgroundPath);

bool isLauncherTmdPatched(const char* path);

UNLAUNCH_VERSION loadUnlaunchInstaller(const char* path);

#ifdef __cplusplus
}
#endif

#endif