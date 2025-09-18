#include <nds.h>
#include <stdbool.h>
#include "nandio.h"

#define STAGE2_HEADER_SECTOR 1
#define NOCASH_FOOTER_SECTOR 2044
#define SECTOR_SIZE 512

#define MBR_PARTITIONS           4
#define MBR_BOOTSTRAP_SIZE       (SECTOR_SIZE - (2 + MBR_PARTITIONS * 16))
#define DSI_FAT_COPY_SIZE        52

typedef struct {
	uint8_t head;
	uint8_t sectorAndCylHigh;
	uint8_t cylinderLow;
} PACKED chs_t;

typedef struct {
	uint8_t status;
	chs_t chs_first;
	uint8_t type;
	chs_t chs_last;
	uint32_t offset;
	uint32_t length;
} PACKED mbr_partition_t;


typedef struct {
	uint8_t bootstrap[MBR_BOOTSTRAP_SIZE];
	mbr_partition_t partitions[MBR_PARTITIONS];
	uint8_t boot_signature[2];
} PACKED mbr_t;

static_assert(sizeof(mbr_t) == 512);

static u8 sector_buf[SECTOR_SIZE] __attribute__((aligned(4)));

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

static u8 sectors_fat[2][DSI_FAT_COPY_SIZE][SECTOR_SIZE] __attribute__((aligned(4)));


// The DSi nand uses 2 FAT copies
// Old homebrews only accounted for one of them when performing operations
// leaving the other copy unmodified.
// manually copy the first fat to the 2nd one so that they're back in sync
bool nandio_synchronize_fats()
{
	static unsigned int synchronized = 0;
	if(synchronized != 0)
		return (synchronized - 1) != 0;

	if(!nand_ReadSectorsCrypt(0, 1, sector_buf))
	{
		synchronized = 1;
		return false;
	}
	mbr_t* mbr = (mbr_t*)sector_buf;
	sec_t fat_offset = mbr->partitions[0].offset;
	
	nand_ReadSectorsCrypt(fat_offset, 1, sector_buf);

	u8 number_of_fats = sector_buf[0x10];
	
	if(number_of_fats != 2)
	{
		synchronized = 1;
		return false;
	}

	u8 reserved_sectors = sector_buf[0x0E];
	u16 sectors_per_fat = sector_buf[0x16] | ((u16)sector_buf[0x17] << 8);

	if(sectors_per_fat != DSI_FAT_COPY_SIZE)
	{
		synchronized = 1;
		return false;
	}

	sec_t fat_start = fat_offset + reserved_sectors;
	nand_ReadSectorsCrypt(fat_start, DSI_FAT_COPY_SIZE * 2, sectors_fat);
	for (u32 sector = 0; sector < DSI_FAT_COPY_SIZE; sector++)
	{
		if (memcmp(sectors_fat[0][sector], sectors_fat[1][sector], SECTOR_SIZE) != 0)
		{
			nand_WriteSectorsCrypt(fat_start + sector + DSI_FAT_COPY_SIZE, 1, sector_buf);
		}
	}
	synchronized = 2;
	return true;
}
