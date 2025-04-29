#ifndef MESSAGE_H
#define MESSAGE_H

#include <nds/ndstypes.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	YES = true,
	NO = false
};

void keyWait(u32 key);
bool choiceBox(const char* message);
bool choicePrint(const char* message);
bool randomConfirmBox(const char* message);
void messageBox(const char* message);
void messagePrint(const char* message);

#ifdef __cplusplus
}
#endif

#endif