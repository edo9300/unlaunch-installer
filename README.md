# "Safe" unlaunch installer
A basic homebrew with a single job, install and uninstall unlaunch from a console.

## WARNING
This can modify your internal system NAND! There is *always* a risk of
**bricking**, albeit small, when you modify NAND. Please proceed with caution.

## Features
- Safety checks to install and uninstall unlaunch without a risk of bricking
- Fully compatible with older unlaunch installs
- Works on every DSi, whether it's retail or development
- Keeps a recovery copy of unlaunch in NAND to protect against future bricks
  (only on retail consoles)

## Notes
This installer comes bundled with a specific version of unlaunch (2.0), but can
load a separate unlaunch installer from the root of the sd card, named
`unlaunch.dsi`.

Supported unlaunch versions are 1.8, 1.9 and 2.0, since earlier ones don't work
if installed with this new method.

Due to some unforunate version differences, the install method used by this
application won't be usable on consoles with firmware 1.4.2 (1.4.3 for china).
So installing on consoles that ship that version will be prevented

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
The installer ships with 2 binary patches, one is an "ahestetic" one to enable
the dsi H&S screen and sound.
The other is a mandatory one, required to make unlaunch properly use the tmd that
was installed in the HNAA folder, otherwise it would attempt to save its settings
to random fat blocks, since it assumes that unlaunch is installed in the blocks right
after the title.tmd associated with the launcher read from HWINFO_S.
More specifically, the sound and splash patch modifies the arm7 instruction of unlaunch at
address 0x1308 (in the then relocated code it is run at 0x23fe038). The patched instruction
is a `bl` to the function patching a second binary blob of the launcher, and is replaced with
a nop.
The other patch, modifies the code responsible for reading the launcher title id from HWINFO.
The original code is as follow
```
@ this loads in r0 the buffer containing the contents read
@ from HWINFO_S, offsetted at the position of the title id
FC 05 9F E5    ldr   r0, [pc, #0x5fc]
FC 15 9F E5    ldr   r1, [pc, #0x5fc]
@ This reads the title id and then stores it to the address
@ pointed by r1, which is then used across the whole program
00 00 90 E5    ldr   r0, [r0]
00 00 81 E5    str   r0, [r1]
F4 05 9F E5    ldr   r0, [pc, #0x5f4]
9d 01 00 EB    bl    #0x602a3c4
C5 FD FF EB    bl    #0x602946c
EC 05 9F E5    ldr   r0, [pc, #0x5ec]
00 00 90 E5    ldr   r0, [r0]
E8 15 9F E5    ldr   r1, [pc, #0x5e8]
66 00 00 EB    bl    #0x6029f00
E4 05 9F E5    ldr   r0, [pc, #0x5e4]
E4 15 9F E5    ldr   r1, [pc, #0x5e4]
14 20 A0 E3    mov   r2, #0x14
0C 09 00 EB    bl    #0x602c1a8
DC 05 9F E5    ldr   r0, [pc, #0x5dc]
91 01 00 EB    bl    #0x602a3c4
BA FD FF EB    bl    #0x602946c
D4 05 9F E5    ldr   r0, [pc, #0x5d4]
00 00 90 E5    ldr   r0, [r0]
D0 15 9F E5    ldr   r1, [pc, #0x5d0]
02 2C A0 E3    mov   r2, #0x200
5B 00 00 EB    bl    #0x6029f04
C8 15 9F E5    ldr   r1, [pc, #0x5c8]
E4 01 91 E5    ldr   r0, [r1, #0x1e4]
72 08 00 EB    bl    #0x602bf6c
C0 15 9F E5    ldr   r1, [pc, #0x5c0]
19 00 00 EB    bl    #0x6029e10
00 00 A0 E3    mov   r0, #0
01 00 C1 E4    strb  r0, [r1], #1
B4 05 9F E5    ldr   r0, [pc, #0x5b4]
93 01 D0 E5    ldrb  r0, [r0, #0x193]
01 00 C1 E4    strb  r0, [r1], #1
FF 9F BD E8    pop   {r0, r1, r2, r3, r4, r5, r6, r7, r8, sb, sl, fp, ip, pc}
```
The patched code is
```
@ Instead of reading the title id from the HWINFO_S buffer,
@ read the hardcoded value placed at the bottom.
@ all the relative addresses have been incremented by 4 to account
@ for the shift of the instructions (same for the relative bl instructions)
7C 00 9F E5    ldr     r0, [pc, #0x7c]
FC 15 9F E5    ldr     r1, [pc, #0x5fc]
00 00 81 E5    str     r0, [r1]
F8 05 9F E5    ldr     r0, [pc, #0x5f8]
9D 01 00 EB    bl      #0x602a3c4
C6 FD FF EB    bl      #0x602946c
F0 05 9F E5    ldr     r0, [pc, #0x5f0]
00 00 90 E5    ldr     r0, [r0]
EC 15 9F E5    ldr     r1, [pc, #0x5ec]
67 00 00 EB    bl      #0x6029f00
E8 05 9F E5    ldr     r0, [pc, #0x5e8]
E8 15 9F E5    ldr     r1, [pc, #0x5e8]
14 20 A0 E3    mov     r2, #0x14
0D 09 00 EB    bl      #0x602c1a8
E0 05 9F E5    ldr     r0, [pc, #0x5e0]
92 01 00 EB    bl      #0x602a3c4
BB FD FF EB    bl      #0x602946c
D8 05 9F E5    ldr     r0, [pc, #0x5d8]
00 00 90 E5    ldr     r0, [r0]
D4 15 9F E5    ldr     r1, [pc, #0x5d4]
02 2C A0 E3    mov     r2, #0x200
5C 00 00 EB    bl      #0x6029f04
CC 15 9F E5    ldr     r1, [pc, #0x5cc]
E8 01 91 E5    ldr     r0, [r1, #0x1e8]
73 08 00 EB    bl      #0x602bf6c
C4 15 9F E5    ldr     r1, [pc, #0x5c4]
1A 00 00 EB    bl      #0x6029e10
00 00 A0 E3    mov     r0, #0
01 00 C1 E4    strb    r0, [r1], #1
B8 05 9F E5    ldr     r0, [pc, #0x5b8]
97 01 D0 E5    ldrb    r0, [r0, #0x197]
01 00 C1 E4    strb    r0, [r1], #1
FF 9F BD E8    pop     {r0, r1, r2, r3, r4, r5, r6, r7, r8, sb, sl, fp, ip, pc}
41 41 4E 48    .word 0x484e4141
```
The patched code is found at address `0x60a7` in the decompressed binary, and is loaded into
ram at address `0x6029d38`.

The lzss compressed arm7 payload is found at offset `0x8580` for unlaunch 2.0 with a length of `0x67FD`.

## Credits
- [DevkitPro](https://devkitpro.org/): devkitARM and libnds
- [Martin Korth (nocash)](https://problemkaputt.de):
  [GBATEK](https://problemkaputt.de/gbatek.htm)
- [JeffRuLz](https://github.com/JeffRuLz)/[Epicpkmn11](https://github.com/Epicpkmn11):
  [TMFH](https://github.com/JeffRuLz/TMFH)/[NTM](https://github.com/Epicpkmn11/NTM)
  (what this is project used as base for menus and other things)
- [rvtr](https://github.com/rvtr):
   Adding support for installing to dev/proto consoles and the ( ͡° ͜ʖ ͡°) icon