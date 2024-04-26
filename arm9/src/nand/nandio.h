#pragma once

#include <stdint.h>
#include <nds/disc_io.h>

#ifdef __cplusplus
extern "C" {
#endif

/************************ Constants / Defines *********************************/

#define CRYPT_BUF_LEN         64
#define NAND_DEVICENAME       (('N' << 24) | ('A' << 16) | ('N' << 8) | 'D')

extern const DISC_INTERFACE   io_dsi_nand;

/************************ Function Protoypes **********************************/

void nandio_set_fat_sig_fix(uint32_t offset);


extern bool nandio_shutdown();

extern bool nandio_lock_writing();
extern bool nandio_unlock_writing();
extern bool nandio_force_fat_fix();
extern void nandio_synchronize_fats();

#ifdef __cplusplus
}
#endif
