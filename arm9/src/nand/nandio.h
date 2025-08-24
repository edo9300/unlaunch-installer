#pragma once

#include <stdint.h>
#include <nds/disc_io.h>
#include "../nocashFooter.h"

#ifdef __cplusplus
extern "C" {
#endif

extern void nandio_construct_nocash_footer(NocashFooter* footer);
extern bool nandio_read_nocash_footer(NocashFooter* footer);
extern bool nandio_write_nocash_footer(NocashFooter* footer);

extern void nandio_calculate_stage2_sha(void* digest);

#ifdef __cplusplus
}
#endif
