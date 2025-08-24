#include <nds.h>
#include <stdbool.h>
#include "nandio.h"

#define STAGE2_HEADER_SECTOR 1
#define NOCASH_FOOTER_SECTOR 2044
#define SECTOR_SIZE 512

static u32 sector_buf32[SECTOR_SIZE/sizeof(u32)];
static u8 *sector_buf = (u8*)sector_buf32;

void getCID(u8 *CID)
{
	vu8* CIDbuff = (vu8*)0x2FFD7BC;
	for(int i = 0; i < 16; ++i)
	{
		CID[i] = CIDbuff[i];
	}
}

void getConsoleID(u8 *consoleID)
{
	vu8 *fifo=(vu8*)0x02300000;
	for(size_t i = 0; i < 8; ++i)
	{
		consoleID[i] = fifo[i];
	}
}

void nandio_construct_nocash_footer(NocashFooter* footer)
{	
	u8 CID[16];
	u8 consoleID[8];
	
	getCID(CID);
	getConsoleID(consoleID);
	
	constructNocashFooter(footer, CID, consoleID);
}

bool nandio_read_nocash_footer(NocashFooter* footer)
{
	if(!nand_ReadSectors(NOCASH_FOOTER_SECTOR, 1, sector_buf))
	{
		return false;
	}

	memcpy(footer, sector_buf, sizeof(NocashFooter));
	return true;
}

bool nandio_write_nocash_footer(NocashFooter* footer)
{
	if(!nand_ReadSectors(NOCASH_FOOTER_SECTOR, 1, sector_buf))
	{
		return false;
	}
	
	memcpy(sector_buf, footer, sizeof(NocashFooter));
	
	
	if(!nand_WriteSectors(NOCASH_FOOTER_SECTOR, 1, sector_buf))
	{
		return false;
	}
	
	return true;
}

void nandio_calculate_stage2_sha(void* digest)
{
	if(!nand_ReadSectors(STAGE2_HEADER_SECTOR, 1, sector_buf))
	{
		return;
	}
	swiSHA1Calc(digest, sector_buf, SECTOR_SIZE);
}
