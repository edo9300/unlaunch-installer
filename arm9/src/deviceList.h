#ifndef DEVICE_LIST_H
#define DEVICE_LIST_H
#include <nds/memory.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PACKED Device {
	char driveLetter;
	struct PACKED {
		u8 phisicalDrive	: 1; // (0=External SD/MMC Slot, 1=Internal eMMC)
		u8 zero1 			: 2; // (maybe MSBs of Drive)
		u8 deviceType		: 2; // (0=Physical, 1=Virtual/File, 2=Virtual/Folder, 3=Reserved)
		u8 partition		: 1; // (0=1st, 1=2nd)
		u8 zero2 			: 1; // (maybe MSB of Partition)
		u8 encrypt 			: 1; // (set for eMMC physical devices; not for virtual, not for SD)
	};
	u8 accessRights; // (bit1=Write, bit2=Read)
	u8 zero;
	char deviceName[0x10]; // (eg. "nand" or "dataPub") (zeropadded)
	char path[0x40]; // (eg. "/" or "nand:/shared1") (zeropadded)
} Device;


typedef struct PACKED DeviceList {
	Device devices[11];
	u8 zerofilled[0x24];
	char appname[0x40];
} DeviceList;

DeviceList* getDeviceList(void);

#ifdef __cplusplus
}
#endif

#endif