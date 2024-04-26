#include "deviceList.h"
#include <string.h>

#define DEVICE_LIST_SENTINEL *(vu32*)0x02300020
#define DEVICE_LIST_ADDR (vu8*)0x02300024

DeviceList* getDeviceList(void)
{
	static bool gotDeviceList = false;
	if(!gotDeviceList) {
		while(!DEVICE_LIST_SENTINEL);
		DeviceList* list = (DeviceList*)DEVICE_LIST_ADDR;
		bool isSd = strncmp(list->appname, "sdmc", 4) == 0;
		if(!isSd && strncmp(list->appname, "nand", 4) != 0)
			return 0;
		if(isSd)
		{
			//transform sdmc:/ to sd:/
			memmove(list->appname + 2, list->appname + 4, sizeof(list->appname) - 4);
			list->appname[sizeof(list->appname) - 1] = '\0';
			list->appname[sizeof(list->appname) - 2] = '\0';
		}
		gotDeviceList = true;
	}
	
	return (DeviceList*)DEVICE_LIST_ADDR;
}