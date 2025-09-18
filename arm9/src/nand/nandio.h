#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "../nocashFooter.h"

#ifdef __cplusplus
extern "C" {
#endif

extern bool nandio_read_nocash_footer(NocashFooter* footer);
extern bool nandio_write_nocash_footer(NocashFooter* footer);

extern void nandio_calculate_stage2_sha(void* digest);

extern bool nandio_synchronize_fats();

#ifdef __cplusplus
}
#endif
