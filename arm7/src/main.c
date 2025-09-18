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
#include <nds.h>
#include <string.h>

volatile bool exitflag = false;
volatile bool reboot = false;

TWL_CODE void i2cIRQHandlerCustom()
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

int main()
{
	irqInit();
	irqSetAUX(IRQ_I2C, i2cIRQHandlerCustom);
	fifoInit();

	if (isDSiMode())
	{
		*((vu64*)0x02300000) = getConsoleID();

		SDMMC_init(SDMMC_DEV_eMMC);
		SDMMC_getCidRaw(SDMMC_DEV_eMMC, (vu32*)0x2FFD7BC);
	}
	
	fifoSendValue32(FIFO_USER_02, 42);

	installSystemFIFO();

	irqSet(IRQ_VBLANK, inputGetAndSend);

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
