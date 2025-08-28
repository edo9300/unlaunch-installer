#ifndef CONSOLE_ID_H
#define CONSOLE_ID_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void getConsoleID(volatile uint8_t ConsoleIdOut[8]);

#ifdef __cplusplus
}
#endif

#endif //CONSOLE_ID_H
