#ifndef MAIN_H
#define MAIN_H

#include <nds.h>
#include <fat.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

extern volatile bool programEnd;
extern volatile bool charging;
extern volatile u8 batteryLevel;

extern PrintConsole topScreen;
extern PrintConsole bottomScreen;

extern int bgGifTop;
extern int bgGifBottom;

void clearScreen(PrintConsole* screen);

#ifdef __cplusplus
}
#endif

#endif
