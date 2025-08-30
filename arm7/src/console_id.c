#include "console_id.h"
#include <stdint.h>
#include <string.h>
#include <nds.h>

static void computeAes(void* out)
{
	REG_AES_BLKCNT = (1 << 16);
	REG_AES_CNT = ( AES_CNT_MODE(2) |
					AES_WRFIFO_FLUSH |
					AES_RDFIFO_FLUSH |
					AES_CNT_KEY_APPLY |
					AES_CNT_KEYSLOT(3) |
					AES_CNT_ENABLE
					);
	
	uint32_t* out32 = (uint32_t*)out;

	for (int i = 0; i < 4; ++i) 
		REG_AES_WRFIFO = 0;
	while (((REG_AES_CNT >> 0x5) & 0x1F) < 0x4); //wait for every word to get processed
	for (int i = 0; i < 4; ++i)
		out32[i] = REG_AES_RDFIFO;
}

void computeConsoleIdFromKeyX(aes_keyslot_t* keyslot, volatile uint8_t ConsoleIdOut[8])
{
	uint8_t canary[16]={0};
	computeAes(canary);

	uint8_t scratch[0x10];
	uint8_t key_y_oracle;
	for (int j = 0; j < 256; j++)
	{
		keyslot->key_y[15] = j & 0xFF;
		computeAes(scratch);
		if (!memcmp(scratch, canary, 16))
		{
			key_y_oracle = j;
			break;
		}
	}

	for (int i = 0; i < 8; i++)
	{
		for (int j = 0; j < 256; j++)
		{
			int key_x_idx = i;
			if(i >= 4)
				key_x_idx += 8;
			keyslot->key_x[key_x_idx] = j & 0xFF;
			keyslot->key_y[15] = key_y_oracle;
			computeAes(scratch);
			if (!memcmp(scratch, canary, 16))
			{
				ConsoleIdOut[i] = j;
				break;
			}
		}
	}
}

void getConsoleID(volatile uint8_t ConsoleIdOut[8])
{
	// always "enable" the keyslot 3 for nand crypto, so that the keys are properly derived
	((volatile  uint32_t*)(AES_KEYSLOT3.key_y))[3] = 0xE1A00005;
	// first check whether we can read the console ID directly and it was not hidden by SCFG
	if ((REG_SCFG_ROM & (1u << 10)) == 0 && ((*(volatile uint8_t*)0x04004D08) & 0x1) == 0)
	{
		// The console id registers are readable, so use them!
		uint64_t consoleId = REG_CONSOLEID;
		for(int i = 0; i < 8; ++i) {
			ConsoleIdOut[i] = consoleId & 0xFF;
			consoleId >>= 8;
		}
		if(ConsoleIdOut[0] != 0 && ConsoleIdOut[1] != 0)
			return;
	}
	
	computeConsoleIdFromKeyX(&AES_KEYSLOT3, ConsoleIdOut);
}
