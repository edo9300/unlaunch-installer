# "Safe" unlaunch installer
A basic homebrew with a single job, install and uninstall unlaunch from a console.

## Features
- Safety checks to install and uninstall unlaunch without a risk of bricking
- Fully compatible with older unlaunch installs
- Works on every DSi, whether it's retail or development
- Keeps a recovery copy of unlaunch in NAND to protect against future bricks
  (only on retail consoles)

## Building
Blocksds 1.23.0 or later is required due to bugs in the `ftruncate` function,
once set up, just run `make` in the root folder.

## Notes
This installer only supports installing unlaunch 2.0 (which comes bundled with the nds),
since there are binary patched applied that for ease of use are only implemented for that
version.

Due to some unforunate version differences, the install method used by this
application won't be usable on consoles with firmware 1.4.2 (1.4.3 for china and korea).
So installing on consoles that ship that version won't be allowed

## Differences with official installer
The Nintendo DSi's stage2 has a failsafe where it will load a backup launcher
(HNAA) if it can't find the real one (eg. missing, corrupted), even on retail
consoles.

The official installer will append itself to the tmd of the "real" launcher.
This leaves the console bricked in case of failed install or uninstall (since
the backup launcher doesn't exist, stage2 can't save you).
The safe unlaunch installer takes advantage of this feature by installing
unlaunch to the backup launcher.
The official launcher's TMD is "broken" by changing a byte, making stage2
load unlaunch in the backup launcher. 

This is safer than normal unlaunch installs because as long as the main TMD
isn't touched, the system can't be bricked by those operations (apart from
total nand failure), and when the main TMD is tampered with, unlaunch is
already there as fallback in case of errors.
Uninstalling is also safer as this program only has to restore that
previously changed byte to restore the main TMD.
This allows to keep backup unlaunch "installed" as general brick
protection since it won't interfere with the system. As a bonus, if you
sell/trade you console in the future and the new owner uses the official
installer, they'll be protected from bricks.

## Patches applied to unlaunch
The installer ships with 6 binary patches:
 - Enable **both** the dsi H&S screen and sound. This works by modifying
 the arm7 instruction of unlaunch at address 0x1308 (in the then relocated code it is
 run at 0x23fe038). The patched instruction is a `bl` to the function patching a second
 binary blob of the launcher, and is replaced with a nop.
 - Enable the dsi menu H&S screen **only**. This works by modifying
 the patches to apply to the launcher's arm7i binary by only making unlaunch patch the code
 disabling sound initialization.
 - Enable the dsi menu sound **only**. This works by modifying
 the patches to apply to the launcher's arm7i binary by only making unlaunch patch the code
 disabling the display of the H&S screen.
 - A mandatory one, required to make unlaunch properly use the tmd that
 was installed in the HNAA folder, rather than deducing it from parsing HWINFO_S.
This makes so that unlaunch writes the options to the actual tmd rather than to the extra
clusters in the main launcher's tmd, it also allows again wifiboot to be launched.
 - Fix the a wrong behaviour regarding homebrew booting and passing to them the directory they were in.
The patch replaces a beq at file offset `0x2cf0` (0x06026980 when executed) with a nop.
The replaced check is for the rom's tid to be of "dsiware" (which almost no homebrew is)
 - Overwrites the dsi launcher detection logic to trick unlaunch into not patching it with its patches.
The patch replaces a bleq at file offset `0x2f8` (0x06023f88 when executed) with a nop.
The replaced jump is the call to the function that should take care of patching the loaded launcher binary.

Every binary patch also includes the device list fix patch

The patches are compressed with `$WONDERFUL_TOOLCHAIN/bin/wf-nnpack-lzss -evo`

## Unlaunch binary patch format

When booting the dsi launcher, unlaunch applies 2 set of binary patches to its arm9 and arm7i binaries
(once decrypted from their modcrypt encryption).

The patches are located at offset `0x154C`, with size `0x1C6` and `0x1712`, with size `0x3E` respectively.

A single patch is structured as such:
```
u16 dataToMatchLength;
u16 offsetInMatchedBlockToStartPatching;
u16 dataToWriteLength;
u16 dataToMatch[dataToMatchLength & 0x7FFF];
u16 dataToWrite[dataToWriteLength];
```
a `dataToMatchLength` of `0` means there's no more blocks to parse.

If a short to match is `0xAAAA`, that has to be interpreted as wildcard matching any pattern.

In its patching code, unlaunch checks if bit `15` of `dataToMatchLength` is set, in which case it will skip
that patch block unless a global variable at address `0x2FFFDF4` has the 2 lower bits clear (`*(u32)0x2FFFDF4 & 3 == 0`).

## Tmd path retrieval patch
This patch makes so that unlaunch reads the path to the launcher tmd from the leftover data left
in DTCM (at address `0x30034C4`, a string containing `title/00030017/484e41XX/content/title.tmd`).

2 pieces of the binary are patched:
 - The first replaces a random instruction in the early arm9 bootstrap to a jump to our custom payload.
 The function in question is the one at ram address `0x602c454` (payload offset `0x8838`), we replace the return statement
 `ldmia      sp!,{r4,r5,r6,r7,r8,r9,r10,r11,r12,pc}` with a `b 0x0602acd0` (the target of our payload)

 - The second patches the function that unlaunch uses to open a file handle to the tmd to read/write to.
 The function in question is at ram address `0x602acc8` (payload offset `0x7038`).
 Basically the function is rewritten, the existing function reads the launcher TID parsed from HWINFO_S
 and formats a string then passed to `fopen`.
```
F0 5F 2D E9     stmdb      sp!,{r4,r5,r6,r7,r8,r9,r10,r11,r12,lr}

70 01 9F E5     ldr        r0,[0x602ae44]
00 00 90 E5     ldr        r0,[r0,#0x0]		@ gets the TID parsed from HWINFO_S
6c 11 9f E5     ldr        r1,[0x602ae48]   @"484E41xx\CONTENT\TITLE.TMD" (note, this is a substring of the string below)
4C FC FF EB     bl         0x06029e10       @function formatting an int to string

68 01 9F E5     ldr        r0,[0x602ae4c]   @"nand:\TITLE\00030017\484E41xx\CONTENT\TITLE.TMD" (note, the string is now formatted)
B7 FD FF EB     bl         0x0602a3c4       @fopen
F0 9F BD E8     ldmia      sp!,{r4,r5,r6,r7,r8,r9,r10,r11,r12,pc}
```

The new payload is 
```
7C 01 9F E5     ldr        r0,[0x602ae4c]   @"nand:\TITLE\00030017\484E41xx\CONTENT\TITLE.TMD" (note, the string is preformatted by the below payload)
BC FD FF EA     b          0x0602a3c4		@fopen

03 20 A0 E3     mov        r2,#0x3          @copy 3 bytes (corresponding to 1XX in the format string), rather than 2 so that the address is aligned
08 10 9F E5     ldr        r1,dst
15 0C 8D E2     add        r0,sp,#0x1500    @when called, sp is 0x3001FD8, by adding 0x1500 we get 0x30034D8, which corresponds to the substring "1XX/content/title.tmd"
31 05 00 EB     bl         0x0602c1a8		@memcpy
F0 9F BD E8     ldmia      sp!,{r4,r5,r6,r7,r8,r9,r10,r11,r12,pc}
dst:
9a 86 02 06     .word      0x0602869a       @"1xx\CONTENT\TITLE.TMD" (note, this is a substring of the string above)
```

The lzss compressed arm7 payload is found at offset `0x8580` for unlaunch 2.0 with a length of `0x67FD`.

## Credits
- [AntonioND](https://github.com/AntonioND/): for [blocksds](https://blocksds.skylyrac.net/)
- [Martin Korth (nocash)](https://problemkaputt.de):
  [GBATEK](https://problemkaputt.de/gbatek.htm) and [UNLAUNCH](https://problemkaputt.de/unlaunch.htm) itself.
- [JeffRuLz](https://github.com/JeffRuLz)/[Epicpkmn11](https://github.com/Epicpkmn11):
  [TMFH](https://github.com/JeffRuLz/TMFH)/[NTM](https://github.com/Epicpkmn11/NTM)
  (what this is project used as base for menus)
- [rvtr](https://github.com/rvtr):
   Adding support for installing to dev/proto consoles and the ( ͡° ͜ʖ ͡°) icon