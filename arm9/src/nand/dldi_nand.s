// SPDX-License-Identifier: Zlib
// SPDX-FileNotice: Modified from the original version by the BlocksDS project.
//
// Copyright (C) 2006-2008 Michael Chisholm (Chishm)
// Copyright (C) 2006-2016 Dave Murphy (WinterMute)

#include <nds/arm9/dldi_asm.h>

    .syntax  unified

    .align  4

    .arm

    .global _io_dldi_stub
    .section .dldi, "ax"

_io_dldi_stub:

dldi_start:

// Driver patch file standard header (16 bytes)

    .word   0xBF8DA5ED        // Magic number to identify this region
    .asciz  "nanddld"         // Identifying Magic string (8 bytes with null terminator)
    .byte   0x01              // Version number
    .byte   __dldi_log2_size  // Log [base-2] of the size of this driver in bytes.
    .byte   0x00              // Sections to fix
    .byte   __dldi_log2_size  // Log [base-2] of the allocated space in bytes.

// Text identifier. Up to 47 chars + terminating null (48 bytes)

    .align  4
    .asciz  "NAND"

// Offsets to important sections within the data (32 bytes)

    .align  6
    .word   dldi_start    // data start
    .word   dldi_data_end // data end
    .word   0x00000000  // Interworking glue start -- Needs address fixing
    .word   0x00000000  // Interworking glue end
    .word   0x00000000  // GOT start               -- Needs address fixing
    .word   0x00000000  // GOT end
    .word   0x00000000  // bss start               -- Needs setting to zero
    .word   0x00000000  // bss end

// DISC_INTERFACE data (32 bytes)

    .ascii  "NAND"      // ioType
    .word   FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE  // Features
    // Function pointers to standard device driver functions
    .word   nandio_startup
    .word   nandio_is_inserted
    .word   nandio_read_sectors
    .word   nandio_write_sectors
    .word   nandio_clear_status
    .word   nandio_shutdown

    .align
    .pool

dldi_data_end:
    .end
