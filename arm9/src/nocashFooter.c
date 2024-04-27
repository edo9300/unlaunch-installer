#include "nocashFooter.h"
#include <string.h>

static const char* nocashMagicString = "DSi eMMC CID/CPU";

bool isFooterValid(const NocashFooter* footer)
{
	return memcmp(nocashMagicString, footer->footerID, sizeof(footer->footerID)) == 0;
}

void constructNocashFooter(NocashFooter* footer, const u8* CID, const u8* consoleID)
{
	memset(footer, 0, sizeof(NocashFooter));
	memcpy(footer->footerID, nocashMagicString, sizeof(footer->footerID));
	memcpy(footer->CID, CID, sizeof(footer->CID));
	memcpy(footer->consoleId, consoleID, sizeof(footer->consoleId));
}