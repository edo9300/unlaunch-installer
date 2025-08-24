# SPDX-License-Identifier: CC0-1.0
#
# SPDX-FileContributor: Antonio Niño Díaz, 2023

BLOCKSDS	?= /opt/blocksds/core
BLOCKSDSEXT	?= /opt/blocksds/external

# User config
# ===========

NAME		:= unlaunch-installer

GAME_TITLE	:= Safe Unlaunch installer
GAME_AUTHOR	:= edo9300
GAME_ICON	:= icon.bmp

GAME_CODE		:= UNLI
GAME_LABEL		:= UNLINS

# Source code paths
# -----------------

# List of folders to combine into the root of NitroFS:
NITROFSDIR	:= nitrofiles

# Tools
# -----

MAKE		:= make
RM		:= rm -rf

# Verbose flag
# ------------

ifeq ($(VERBOSE),1)
V		:=
else
V		:= @
endif

# Directories
# -----------

ARM9DIR		:= arm9
ARM7DIR		:= arm7

# Build artfacts
# --------------

ROM		:= $(NAME).dsi

# Targets
# -------

.PHONY: all clean arm9 arm7

all: $(ROM)

clean:
	@echo "  CLEAN"
	$(V)$(MAKE) -f Makefile.arm9 clean --no-print-directory
	$(V)$(MAKE) -f Makefile.arm7 clean --no-print-directory
	$(V)$(RM) $(ROM) build ntrboot.nds

arm9:
	$(V)+$(MAKE) -f Makefile.arm9 --no-print-directory

arm7:
	$(V)+$(MAKE) -f Makefile.arm7 --no-print-directory

dump:
	$(V)+$(MAKE) -f Makefile.arm7 --no-print-directory dump
	$(V)+$(MAKE) -f Makefile.arm9 --no-print-directory dump

ifneq ($(strip $(NITROFSDIR)),)
# Additional arguments for ndstool
NDSTOOL_ARGS	:= -d $(NITROFSDIR)

# Make the NDS ROM depend on the filesystem only if it is needed
$(ROM): $(NITROFSDIR)
endif

# Combine the title strings
GAME_FULL_TITLE := $(GAME_TITLE);$(GAME_AUTHOR)

$(ROM): arm9 arm7
	@echo "  NDSTOOL $@"
	$(V)$(BLOCKSDS)/tools/ndstool/ndstool -c $@ \
		-7 build/arm7.elf -9 build/arm9.elf \
		-u "00030004" \
		-g "$(GAME_CODE)" "00" "$(GAME_LABEL)" \
		-b $(GAME_ICON) "$(GAME_FULL_TITLE)" \
		$(NDSTOOL_ARGS)
	$(V)cp $(ROM) ntrboot.nds
