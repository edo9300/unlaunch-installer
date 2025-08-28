#include "console_id.h"
#include <stdint.h>
#include <string.h>
#include <nds.h>

// aes key constant value
// 0xFFFEFB4E295902582A680F5F1A4F3E79
static const uint8_t DSi_KEY_MAGIC[16] = {
	0x79, 0x3e, 0x4f, 0x1a, 0x5f, 0x0f, 0x68, 0x2a,
	0x58, 0x02, 0x59, 0x29, 0x4e, 0xfb, 0xfe, 0xff
};

static const uint8_t DSi_NAND_KEY_Y[16] = {
	0x76, 0xdc, 0xb9, 0x0a, 0xd3, 0xc4, 0x4d, 0xbd,
	0x1d, 0xdd, 0x2d, 0x20, 0x05, 0x00, 0xa0, 0xe1
};

static void set_ctr(uint32_t* ctr)
{
	for (int i = 0; i < 4; i++) REG_AES_IV[i] = ctr[3-i];
}

// 10 11  22 23 24 25
//this is sort of a bodged together dsi aes function adapted from this 3ds function
//https://github.com/TiniVi/AHPCFW/blob/master/source/aes.c#L42
//as long as the output changes when keyslot values change, it's good enough.
static void aes(void* in, void* out, void* iv, uint32_t method)
{
	REG_AES_CNT = ( AES_CNT_MODE(method) |
					AES_WRFIFO_FLUSH |
					AES_RDFIFO_FLUSH |
					AES_CNT_KEY_APPLY |
					AES_CNT_KEYSLOT(3) |
					AES_CNT_DMA_WRITE_SIZE(2) |
					AES_CNT_DMA_READ_SIZE(1)
					);

	if (iv != NULL) set_ctr((uint32_t*)iv);
	REG_AES_BLKCNT = (1 << 16);
	REG_AES_CNT |= 0x80000000;
	
	uint32_t* in32 = (uint32_t*)in;
	uint32_t* out32 = (uint32_t*)out;

	for (int i = 0; i < 4; ++i) 
		REG_AES_WRFIFO = in32[i];
	while (((REG_AES_CNT >> 0x5) & 0x1F) < 0x4); //wait for every word to get processed
	for (int i = 0; i < 4; ++i)
		out32[i] = REG_AES_RDFIFO;
}

// rotate a 128bit, little endian by shift bits in direction of decreasing significance.
static void u128_rrot(uint8_t *num, uint32_t shift)
{
	uint8_t tmp[16];
	for (int i = 0; i < 16 ;i++)
	{
		// rot: rotate to less significant.
		// LSB is tmp[0], MSB is tmp[15]
		const uint32_t byteshift = shift / 8;
		const uint32_t bitshift = shift % 8;
		tmp[i] = (num[(i+byteshift) % 16] >> bitshift)
			| ((num[(i+byteshift+1) % 16] << (8-bitshift)) & 0xff);
	}
	memcpy(num, tmp, 16);
}

// xor two 128bit, little endian values and store the result into the first
static void u128_xor(uint8_t *a, const uint8_t *b)
{
	for (int i=0;i<16;i++)
	{
		a[i] = a[i] ^ b[i];
	}
}

// sub two 128bit, little endian values and store the result into the first
static void u128_sub(uint8_t *a, const uint8_t *b)
{
	uint8_t carry = 0;
	for (int i = 0; i < 16; i++)
	{
		uint16_t sub = a[i] - b[i] - (carry & 1);
		a[i] = sub & 0xff;
		carry = sub >> 8;
	}
}

//F_XY_reverse does the reverse of F(X^Y): takes (normal)key, and does F in reverse to generate the original X^Y key_xy.
static void F_XY_reverse(const uint8_t *key, uint8_t *key_xy)
{
	memcpy(key_xy, key, 16);
	u128_rrot(key_xy, 42);
	u128_sub(key_xy, DSi_KEY_MAGIC);
}


void getConsoleID(volatile uint8_t ConsoleIdOut[8])
{
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
	// For getting ConsoleID without reading from 0x4004D00...
	uint8_t canary[16]={0};
	uint8_t in[16]={0};
	uint8_t iv[16]={0};

	aes(in, canary, iv, 2);

	uint8_t computedNormalKey[16];
	uint8_t scratch[0x10];
	//write consecutive 0-255 values to any byte in key3 until we get the same aes output as "canary" above - this reveals the hidden byte. this way we can uncover all 16 bytes of the key3 normalkey pretty easily.
	//greets to Martin Korth for this trick https://problemkaputt.de/gbatek.htm#dsiaesioports (Reading Write-Only Values)
	for (int i = 0; i < 16; i++)
	{
		for (int j = 0; j < 256; j++)
		{
			AES_KEYSLOT3.normalkey[i] = j & 0xFF;
			aes(in, scratch, iv, 2);
			if (!memcmp(scratch, canary, 16))
			{
				computedNormalKey[i]=j;
				break;
			}
		}
	}

	uint8_t key_x[16];////key3_x - contains a DSi console id (which just happens to be the LFCS on 3ds)
	F_XY_reverse(computedNormalKey, key_x); //work backwards from the normalkey to get key_x that has the consoleID

	u128_xor(key_x, DSi_NAND_KEY_Y);
	
	for(int i = 0; i < 4; ++i) {
		ConsoleIdOut[i] = key_x[i];
	}
	
	for(int i = 4; i < 8; ++i) {
		ConsoleIdOut[i] = key_x[i + 8];
	}
}
