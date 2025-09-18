#include <nds.h>

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

	struct {
		u64 consoleId;
		u32 cid[4];
	} consoleIdAndCid;
	consoleIdAndCid.consoleId = getConsoleID();

	SDMMC_init(SDMMC_DEV_eMMC);
	SDMMC_getCidRaw(SDMMC_DEV_eMMC, consoleIdAndCid.cid);
	fifoSendDatamsg(FIFO_USER_02, sizeof(consoleIdAndCid), (u8*)&consoleIdAndCid);

	installSystemFIFO();

	irqSet(IRQ_VBLANK, inputGetAndSend);

	irqEnable(IRQ_VBLANK);

	while (!exitflag)
	{
		if (0 == (REG_KEYINPUT & (KEY_SELECT | KEY_START | KEY_L | KEY_R)))
		{
			exitflag = true;
			reboot = true;
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
