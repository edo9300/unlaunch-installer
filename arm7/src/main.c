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
#include "my_sdmmc.h"

#include "deviceList.h"
#include <nds.h>
#include <string.h>

//---------------------------------------------------------------------------------
void VcountHandler()
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

void set_ctr(u32* ctr)
{
	for (int i = 0; i < 4; i++) REG_AES_IV[i] = ctr[3-i];
}

// 10 11  22 23 24 25
//this is sort of a bodged together dsi aes function adapted from this 3ds function
//https://github.com/TiniVi/AHPCFW/blob/master/source/aes.c#L42
//as long as the output changes when keyslot values change, it's good enough.
void aes(void* in, void* out, void* iv, u32 method)
{
	REG_AES_CNT = ( AES_CNT_MODE(method) |
					AES_WRFIFO_FLUSH |
					AES_RDFIFO_FLUSH |
					AES_CNT_KEY_APPLY |
					AES_CNT_KEYSLOT(3) |
					AES_CNT_DMA_WRITE_SIZE(2) |
					AES_CNT_DMA_READ_SIZE(1)
					);

	if (iv != NULL) set_ctr((u32*)iv);
	REG_AES_BLKCNT = (1 << 16);
	REG_AES_CNT |= 0x80000000;

	for (int j = 0; j < 0x10; j+=4) REG_AES_WRFIFO = *((u32*)(in+j));
	while (((REG_AES_CNT >> 0x5) & 0x1F) < 0x4); //wait for every word to get processed
	for (int j = 0; j < 0x10; j+=4) *((u32*)(out+j)) = REG_AES_RDFIFO;
	//REG_AES_CNT &= ~0x80000000;
	//if (method & (AES_CTR_DECRYPT | AES_CTR_ENCRYPT)) add_ctr((u8*)iv);
}

int my_sdmmc_nand_startup();

#define DEVICE_LIST_SENTINEL *(vu32*)0x02300020

//---------------------------------------------------------------------------------
int main()
//---------------------------------------------------------------------------------
{
	DEVICE_LIST_SENTINEL = 0;
	// clear sound registers
	dmaFillWords(0, (void*)0x04000400, 0x100);

	REG_SOUNDCNT |= SOUND_ENABLE;
	writePowerManagement(PM_CONTROL_REG, ( readPowerManagement(PM_CONTROL_REG) & ~PM_SOUND_MUTE ) | PM_SOUND_AMP );
	powerOn(POWER_SOUND);

	readUserSettings();
	ledBlink(0);

	irqInit();
	irqSetAUX(IRQ_I2C, i2cIRQHandlerCustom);
	// Start the RTC tracking IRQ
	initClockIRQ();
	fifoInit();
	touchInit();

	if (isDSiMode() /*|| ((REG_SCFG_EXT & BIT(17)) && (REG_SCFG_EXT & BIT(18)))*/)
	{
		if(__DeviceList)
			memmove((vu8*)0x02300024, __DeviceList, sizeof(DeviceList));
		else
			memset((vu8*)0x02300024, 0, sizeof(DeviceList));
		DEVICE_LIST_SENTINEL = 1;
		
		vu8 *out=(vu8*)0x02300000;
		memset(out, 0, 16);

		// first check whether we can read the console ID directly and it was not hidden by SCFG
		if (((*(vu16*)0x04004000) & (1u << 10)) == 0 && ((*(vu8*)0x04004D08) & 0x1) == 0)
		{
			// The console id registers are readable, so use them!
			memcpy(out, (vu8*)0x04004D00, 8);
		}
		if(out[0] == 0 || out[1] == 0) {
			// For getting ConsoleID without reading from 0x4004D00...
			u8 base[16]={0};
			u8 in[16]={0};
			u8 iv[16]={0};
			u8 *scratch=(u8*)0x02300200;
			u8 *key3=(u8*)0x40044D0;

			aes(in, base, iv, 2);

			//write consecutive 0-255 values to any byte in key3 until we get the same aes output as "base" above - this reveals the hidden byte. this way we can uncover all 16 bytes of the key3 normalkey pretty easily.
			//greets to Martin Korth for this trick https://problemkaputt.de/gbatek.htm#dsiaesioports (Reading Write-Only Values)
			for (int i=0;i<16;i++)
			{
				for (int j=0;j<256;j++)
				{
					*(key3+i)=j & 0xFF;
					aes(in, scratch, iv, 2);
					if (!memcmp(scratch, base, 16))
					{
						out[i]=j;
						//hit++;
						break;
					}
				}
			}
		}

		my_sdmmc_nand_startup();
		my_sdmmc_get_cid(true, (u32*)0x2FFD7BC);	// Get eMMC CID
		//sdmmc_nand_cid((u32*)0x2FFD7BC);
	}

	SetYtrigger(80);

	installSoundFIFO();

	installSystemFIFO();

	irqSet(IRQ_VCOUNT, VcountHandler);

	irqEnable( IRQ_VBLANK | IRQ_VCOUNT | IRQ_NETWORK);

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
