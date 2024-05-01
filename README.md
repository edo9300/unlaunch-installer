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

## Credits
- [DevkitPro](https://devkitpro.org/): devkitARM and libnds
- [Martin Korth (nocash)](https://problemkaputt.de):
  [GBATEK](https://problemkaputt.de/gbatek.htm)
- [JeffRuLz](https://github.com/JeffRuLz)/[Epicpkmn11](https://github.com/Epicpkmn11):
  [TMFH](https://github.com/JeffRuLz/TMFH)/[NTM](https://github.com/Epicpkmn11/NTM)
  (what this is project used as base for menus and other things)
- [rvtr](https://github.com/rvtr):
   Adding support for installing to dev/proto consoles and the ( ͡° ͜ʖ ͡°) icon