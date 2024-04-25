#ifndef UNLAUNCH_H
#define UNLAUNCH_H
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool uninstallUnlaunch(bool notProto, bool hasHNAABackup, const char* retailLauncherTmdPath);
bool installUnlaunch(bool retailConsole, const char* retailLauncherTmdPath);

bool isLauncherTmdPatched(const char* path);

bool loadUnlaunchInstaller(const char* path);

#ifdef __cplusplus
}
#endif

#endif