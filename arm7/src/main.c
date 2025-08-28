/*---------------------------------------------------------------------------------

	default ARM7 core

		Copyright (C) 2005 - 2010
		Michael Noland (joat)
		Jason Rogers (dovoto)
		Dave Murphy (WinterMute)

	This software is provided 'as-is', without any express or implied
	warranty.  In no event will the authors be held liable for any
	damages arising from the use of this software.

	Permission is granted to anyone to use this software for any
	purpose, including commercial applications, and to alter it and
	redistribute it freely, subject to the following restrictions:

	1.	The origin of this software must not be misrepresented; you
		must not claim that you wrote the original software. If you use
		this software in a product, an acknowledgment in the product
		documentation would be appreciated but is not required.

	2.	Altered source versions must be plainly marked as such, and
		must not be misrepresented as being the original software.

	3.	This notice may not be removed or altered from any source
		distribution.

---------------------------------------------------------------------------------*/
#include "deviceList.h"
#include <nds.h>
#include <string.h>
#include "console_id.h"

//---------------------------------------------------------------------------------
void VblankHandler()
//---------------------------------------------------------------------------------
{
	inputGetAndSend();
}

volatile bool exitflag = false;
volatile bool reboot = false;

// Custom POWER button handling, based on the default function:
// https://github.com/devkitPro/libnds/blob/154a21cc3d57716f773ff2b10f815511c1b8ba9f/source/common/interrupts.c#L51-L69
//---------------------------------------------------------------------------------
TWL_CODE void i2cIRQHandlerCustom()
//---------------------------------------------------------------------------------
{
	int cause = (i2cReadRegister(I2C_PM, I2CREGPM_PWRIF) & 0x3) | (i2cReadRegister(I2C_GPIO, 0x02)<<2);

	switch (cause & 3)
	{
		case 1:
			reboot = true;
			exitflag = true;
			break;
		case 2:
			reboot = false;
			exitflag = true;
			break;
	}
}

#define DEVICE_LIST_SENTINEL *(vu32*)0x02300020

//---------------------------------------------------------------------------------
int main()
//---------------------------------------------------------------------------------
{
	DEVICE_LIST_SENTINEL = 0;
	// clear sound registers
	dmaFillWords(0, (void*)0x04000400, 0x100);

	readUserSettings();
	ledBlink(LED_ALWAYS_ON);

	irqInit();
	irqSetAUX(IRQ_I2C, i2cIRQHandlerCustom);
	fifoInit();

	if (isDSiMode() /*|| ((REG_SCFG_EXT & BIT(17)) && (REG_SCFG_EXT & BIT(18)))*/)
	{
		if(__DeviceList)
			memmove((vu8*)0x02300024, __DeviceList, sizeof(DeviceList));
		else
			memset((vu8*)0x02300024, 0, sizeof(DeviceList));
		DEVICE_LIST_SENTINEL = 1;
		
		getConsoleID((vu8*)0x02300000);

		SDMMC_init(SDMMC_DEV_eMMC);
		SDMMC_getCid(SDMMC_DEV_eMMC, CID);
		// the CID returned here is the "correct" version as expected
		// by general tools, we need to convert it back to the "wrong"
		// version matching the direct hw output as that's what used
		// for the cryptographic functions for the dsi nand
		u32 realCID[4];

		realCID[0] = (CID[3] >> 8) | (CID[2] << 24);
		realCID[1] = (CID[2] >> 8) | (CID[1] << 24);
		realCID[2] = (CID[1] >> 8) | (CID[0] << 24);
		realCID[3] = (CID[0] >> 8);
		
		memcpy((u32*)0x2FFD7BC, realCID, sizeof(CID));
		SDMMC_deinit(SDMMC_DEV_eMMC);
	}
	
	fifoSendValue32(FIFO_USER_02, 42);

	SetYtrigger(80);

	installSystemFIFO();

	irqSet(IRQ_VBLANK, VblankHandler);

	irqEnable(IRQ_VBLANK);

	// Keep the ARM7 mostly idle
	int oldBatteryStatus = 0;
	while (!exitflag)
	{
		if ( 0 == (REG_KEYINPUT & (KEY_SELECT | KEY_START | KEY_L | KEY_R)))
		{
			exitflag = true;
		}

		int batteryStatus = i2cReadRegister(I2C_PM, I2CREGPM_BATTERY);
		if(oldBatteryStatus != batteryStatus)
		{
			fifoSendValue32(FIFO_USER_03, oldBatteryStatus = batteryStatus);
		}

		swiWaitForVBlank();
	}

	// Tell ARM9 to safely exit
	fifoSendValue32(FIFO_USER_01, 0x54495845); // 'EXIT'
	fifoWaitValue32(FIFO_USER_02);
	fifoCheckValue32(FIFO_USER_02);

	if (reboot)
	{
		i2cWriteRegister(I2C_PM, I2CREGPM_RESETFLAG, 1);
		i2cWriteRegister(I2C_PM, I2CREGPM_PWRCNT, 1);
	}
	else
	{
		writePowerManagement(PM_CONTROL_REG,PM_SYSTEM_PWR);
	}

	return 0;
}
