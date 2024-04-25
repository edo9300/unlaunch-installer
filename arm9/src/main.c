#include "main.h"
#include "menu.h"
#include "message.h"
#include "nand/nandio.h"
#include "storage.h"
#include "version.h"
#include "unlaunch.h"

volatile bool programEnd = false;
static volatile bool arm7Exiting = false;
static bool unlaunchFound = false;
static bool retailLauncherTmdPresentAndToBePatched = true;
static bool retailConsole = true;
static bool unlaunchInstallerFound = false;
bool charging = false;
u8 batteryLevel = 0;

PrintConsole topScreen;
PrintConsole bottomScreen;

enum {
	MAIN_MENU_SAFE_UNLAUNCH_UNINSTALL,
	MAIN_MENU_SAFE_UNLAUNCH_INSTALL,
	MAIN_MENU_EXIT
};

static void setupScreens()
{
	REG_DISPCNT = MODE_FB0;
	VRAM_A_CR = VRAM_ENABLE;

	videoSetMode(MODE_0_2D);
	videoSetModeSub(MODE_0_2D);

	vramSetBankA(VRAM_A_MAIN_BG);
	vramSetBankC(VRAM_C_SUB_BG);

	consoleInit(&topScreen, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
	consoleInit(&bottomScreen, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);

	clearScreen(&bottomScreen);

	VRAM_A[100] = 0xFFFF;
}

static int mainMenu(int cursor)
{
	//top screen
	clearScreen(&topScreen);

	iprintf("\t\t\"Safe\" unlaunch installer\n");
	iprintf("\nversion %s\n", VERSION);
	iprintf("\n\n\x1B[41mWARNING:\x1B[47m This tool can write to"
			"\nyour internal NAND!"
			"\n\nThis always has a risk, albeit"
			"\nlow, of \x1B[41mbricking\x1B[47m your system"
			"\nand should be done with caution!\n");
	iprintf("\n\t  \x1B[46mhttps://dsi.cfw.guide\x1B[47m\n");
	iprintf("\x1b[21;0HJeff - 2018-2019");
	iprintf("\x1b[22;0HPk11 - 2022-2023");
	iprintf("\x1b[23;0Hedo9300 - 2024");

	//menu
	Menu* m = newMenu();
	setMenuHeader(m, "MAIN MENU");

	char uninstallStr[32], installStr[32];
	sprintf(uninstallStr, "\x1B[%02omUninstall unlaunch", unlaunchFound ? 047 : 037);
	sprintf(installStr, "\x1B[%02omInstall unlaunch", unlaunchInstallerFound && !unlaunchFound ? 047 : 037);
	addMenuItem(m, uninstallStr, NULL, 0);
	addMenuItem(m, installStr, NULL, 0);
	addMenuItem(m, "\x1B[47mExit", NULL, 0);

	m->cursor = cursor;

	//bottom screen
	printMenu(m);

	while (!programEnd)
	{
		swiWaitForVBlank();
		scanKeys();

		if (moveCursor(m))
			printMenu(m);

		if (keysDown() & KEY_A)
			break;
	}

	int result = m->cursor;
	freeMenu(m);

	return result;
}

static void fifoHandlerPower(u32 value32, void* userdata)
{
	if (value32 == 0x54495845) // 'EXIT'
	{
		programEnd = true;
		arm7Exiting = true;
	}
}

static void fifoHandlerBattery(u32 value32, void* userdata)
{
	batteryLevel = value32 & 0xF;
	charging = (value32 & BIT(7)) != 0;
}

int main(int argc, char **argv)
{
	keysSetRepeat(25, 5);
	setupScreens();

	fifoSetValue32Handler(FIFO_USER_01, fifoHandlerPower, NULL);
	fifoSetValue32Handler(FIFO_USER_03, fifoHandlerBattery, NULL);

	//DSi check
	if (!isDSiMode())
	{
		messageBox("\x1B[31mError:\x1B[33m This app is only for DSi.");
		return 0;
	}

	//setup sd card access
	if (!fatInitDefault())
	{
		messageBox("fatInitDefault()...\x1B[31mFailed\n\x1B[47m");
		return 0;
	}

	//setup nand access
	if (!fatMountSimple("nand", &io_dsi_nand))
	{
		messageBox("nand init \x1B[31mfailed\n\x1B[47m");
		return 0;
	}
	
	unlaunchInstallerFound = loadUnlaunchInstaller("sd:/unlaunch.dsi");
	if (!unlaunchInstallerFound)
	{
		messageBox("\x1B[41mWARNING:\x1B[47m unlaunch.dsi was not found in\n"
					"the root of the sd card.\n"
					"Installing unlaunch won't be possible.");
	}

	//check for unlaunch and region
	u8 region = 0xff;
	char retailLauncherTmdPath[64];
	const char* hnaaTmdPath = "nand:/title/00030017/484e4141/content/title.tmd";
	{
		FILE* file = fopen("nand:/sys/HWINFO_S.dat", "rb");
		bool mainTmdIsPatched = false;
		if (file)
		{
			fseek(file, 0xA0, SEEK_SET);
			u32 launcherTid;
			fread(&launcherTid, sizeof(u32), 1, file);
			fclose(file);

			region = launcherTid & 0xFF;

			sprintf(retailLauncherTmdPath, "nand:/title/00030017/%08lx/content/title.tmd", launcherTid);
			unsigned long long tmdSize = getFileSizePath(retailLauncherTmdPath);
			if (tmdSize > 520)
			{
				unlaunchFound = true;
			}
			else if(tmdSize != 520)
			{
				//if size isn't 520 then the tmd either is not present (size 0), or is already invalid, thus no need to patch
				retailLauncherTmdPresentAndToBePatched = false;
			}
			else
			{
				mainTmdIsPatched = isLauncherTmdPatched(retailLauncherTmdPath);
			}
			// HWINFO_S may not always exist (PRE_IMPORT). Fill in defaults if that happens.
		}
		
		// I own and know of many people with retail and dev prototypes
		// These can normally be identified by having the region set to ALL (0x41)
		retailConsole = (region != 0x41 && region != 0xFF);
		
		if (!unlaunchFound && (mainTmdIsPatched || !retailConsole))
		{
			unsigned long long tmdSize = getFileSizePath(hnaaTmdPath);
			if (tmdSize > 520)
			{
				unlaunchFound = true;
			}
		}
	}

	messageBox("\x1B[41mWARNING:\x1B[47m This tool can write to\nyour internal NAND!\n\nThis always has a risk, albeit\nlow, of \x1B[41mbricking\x1B[47m your system\nand should be done with caution!\n\nIf you have not yet done so,\nyou should make a NAND backup.");
	//main menu
	int cursor = 0;

	while (!programEnd)
	{
		cursor = mainMenu(cursor);

		switch (cursor)
		{
			case MAIN_MENU_SAFE_UNLAUNCH_UNINSTALL:
				if(!unlaunchFound)
				{
					messageBox("\x1B[31mError:\x1B[33m Unlaunch is not installed\n");
				} else if(nandio_unlock_writing()) {
					if(uninstallUnlaunch(retailConsole, retailLauncherTmdPath))
					{
						messageBox("Uninstall successful!\n");
						unlaunchFound = false;
					} else {
						messageBox("\x1B[31mError:\x1B[33m Uninstall failed\n");
					}
					nandio_lock_writing();
				}
				break;

			case MAIN_MENU_SAFE_UNLAUNCH_INSTALL:
				if (unlaunchInstallerFound && (choiceBox("Install unlaunch?") == YES)
					&& (retailLauncherTmdPresentAndToBePatched || (choiceBox("There doesn't seem to be a launcher.tmd\nfile matcing the hwinfo file\nKeep installing?") == YES))
					&& nandio_unlock_writing())
				{
					if(installUnlaunch(retailConsole, retailLauncherTmdPresentAndToBePatched ? retailLauncherTmdPath : NULL))
					{
						messageBox("Install successful!\n");
						unlaunchFound = true;
					} else {
						messageBox("\x1B[31mError:\x1B[33m Install failed\n");
					}
					nandio_lock_writing();
				}
				break;

			case MAIN_MENU_EXIT:
				programEnd = true;
				break;
		}
	}

	clearScreen(&bottomScreen);
	printf("Unmounting NAND...\n");
	fatUnmount("nand:");
	printf("Merging stages...\n");
	nandio_shutdown();

	fifoSendValue32(FIFO_USER_02, 0x54495845); // 'EXIT'

	while (arm7Exiting)
		swiWaitForVBlank();

	return 0;
}

void clearScreen(PrintConsole* screen)
{
	consoleSelect(screen);
	consoleClear();
}