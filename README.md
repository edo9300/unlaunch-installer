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

## Differences with official installer
The Nintendo DSi's stage2 has a failsafe where it will load a backup launcher
(HNAA) if it can't find the real one (eg. missing, corrupted), even on retail
consoles.
Whereas the official unlaunch installer appends itself to the tmd of the
primary launcher (thus leaving the console bricked in case of failed install
uninstall), the safe unlaunch installer takes advantage of this feature
by installing unlaunch to the backup launcher.
To make then stage2 actually load unlaunch from the backup launcher,
the main TMD is "broken" by only changing a byte in it so that it skips it and
loads the exploited one from the backup.
This is safer than normal unlaunch installs because as long as the main TMD
isn't touched, the system can't be bricked by those operations (apart from
total nand failure), and the moment the main TMD is tampered with, unlaunch is
already there as fallback in case of errors.
Reverting this operation (thus uninstalling unlaunch) is also safer as this
program only has to restore that previously changed byte to restore the main
TMD, this allows to keep backup unlaunch "installed" as general brick
protection since it won't interfere with the system.

## Credits
- [DevkitPro](https://devkitpro.org/): devkitARM and libnds
- [Martin Korth (nocash)](https://problemkaputt.de):
  [GBATEK](https://problemkaputt.de/gbatek.htm)
- [JeffRuLz](https://github.com/JeffRuLz)/[Epicpkmn11](https://github.com/Epicpkmn11):
  [TMFH](https://github.com/JeffRuLz/TMFH)/[NTM](https://github.com/Epicpkmn11/NTM)
  (what this is project used as base for menus and other things)
- [rvtr](https://github.com/rvtr):
   Adding support for installing to dev/proto consoles and the ( ͡° ͜ʖ ͡°) icon