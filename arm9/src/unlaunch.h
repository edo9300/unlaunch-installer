#ifndef UNLAUNCH_H
#define UNLAUNCH_H
#include <stdbool.h>

bool uninstallUnlaunch(bool notProto, const char* retailLauncherTmdPath);
bool installUnlaunch(bool retailConsole, const char* retailLauncherTmdPath);

#endif