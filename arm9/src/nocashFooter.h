#ifndef NOCASH_FOOTER_H
#define NOCASH_FOOTER_H
#include <nds/ndstypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NocashFooter {
	char footerID[16]; //DSi eMMC CID/CPU
	u8 CID[16];
	u8 consoleId[8];
	u8 pad[0x18];
} NocashFooter;

bool isFooterValid(const NocashFooter* footer);
void constructNocashFooter(NocashFooter* footer, const u8* CID, const u8* consoleID);

#ifdef __cplusplus
}
#endif

#endif