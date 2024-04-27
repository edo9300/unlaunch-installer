#include "deviceList.h"
#include <string.h>

#define DEVICE_LIST_SENTINEL *(vu32*)0x02300020
#define DEVICE_LIST_ADDR (vu8*)0x02300024

static u8 gotDeviceList = 0;

size_t getDeviceNameLenFromAppName(const char (appname)[0x40])
{
	for(int i = 0; i < 0x40; ++i)
	{
		if(appname[i] == ':')
		{
			return i;
		}
	}
	return 0;
}

DeviceList* getDeviceList(void)
{
	if(!gotDeviceList) {
		gotDeviceList = 1;
		while(!DEVICE_LIST_SENTINEL);
		DeviceList* list = (DeviceList*)DEVICE_LIST_ADDR;

		size_t deviceNameLen = getDeviceNameLenFromAppName(list->appname);

		if(deviceNameLen == 0)
		{
			return 0;
		}

		for(int i = 0; i < 11; ++i)
		{
			Device* device = &list->devices[i];
			if(device->deviceName[deviceNameLen] == '\0' && strncmp(device->deviceName, list->appname, deviceNameLen) == 0)
			{
				gotDeviceList = 2;
				bool isSd = device->phisicalDrive == 0;
				if(isSd)
				{
					//transform the root path to sd:/ (can be sdmc:/, nand:/,  nand2:/, etc)
					memmove(list->appname + 2, list->appname + deviceNameLen, sizeof(list->appname) - deviceNameLen);
					list->appname[0] = 's';
					list->appname[1] = 'd';
					memset(list->appname + (sizeof(list->appname) - deviceNameLen + 2), 0, deviceNameLen - 2);
				}
				break;
			}
		}
	}

	if(gotDeviceList == 1)
		return 0;

	return (DeviceList*)DEVICE_LIST_ADDR;
}