#include "bgMenu.h"
#include "main.h"
#include "menu.h"
#include "message.h"
#include "nand/nandio.h"
#include "storage.h"
#include "version.h"
#include "unlaunch.h"
#include "nitrofs.h"
#include "deviceList.h"

volatile bool programEnd = false;
static volatile bool arm7Exiting = false;
static bool unlaunchFound = false;
static bool hnaaUnlaunchFound = false;
static bool retailLauncherTmdPresentAndToBePatched = true;
static bool retailConsole = true;
static UNLAUNCH_VERSION foundUnlaunchInstallerVersion = INVALID;
static bool disableAllPatches = false;
static bool enableSoundAndSplash = false;
static const char* splashSoundBinaryPatchPath = NULL;
static const char* customBgPath = NULL;
volatile bool charging = false;
volatile u8 batteryLevel = 0;

PrintConsole topScreen;
PrintConsole bottomScreen;

enum {
	MAIN_MENU_SAFE_UNLAUNCH_UNINSTALL,
	MAIN_MENU_CUSTOM_BG,
	MAIN_MENU_TID_PATCHES,
	MAIN_MENU_SOUND_SPLASH_PATCHES,
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

	iprintf("\t\"Safe\" unlaunch installer\n");
	iprintf("\nversion %s\n", VERSION);
	iprintf("\n\n\x1B[41mWARNING:\x1B[47m This tool can write to"
			"\nyour internal NAND!"
			"\n\nThis always has a risk, albeit"
			"\nlow, of \x1B[41mbricking\x1B[47m your system"
			"\nand should be done with caution!\n");
	iprintf("\n\t  \x1B[46mhttps://dsi.cfw.guide\x1B[47m\n");
	iprintf("\x1b[23;0Hedo9300 - 2024");

	//menu
	Menu* m = newMenu();
	setMenuHeader(m, "MAIN MENU");

	char soundPatchesStr[64], tidPatchesStr[32];
	sprintf(tidPatchesStr, "Disable all patches: %s",
						disableAllPatches ? "On" : "Off");
	sprintf(soundPatchesStr, "Enable sound and splash: %s",
							enableSoundAndSplash ? "On" : "Off");
	addMenuItem(m, "Uninstall unlaunch", NULL, unlaunchFound, false);
	addMenuItem(m, "Custom background", NULL, foundUnlaunchInstallerVersion != INVALID, true);
	addMenuItem(m, tidPatchesStr, NULL, foundUnlaunchInstallerVersion == v1_9 || foundUnlaunchInstallerVersion == v2_0, false);
	addMenuItem(m, soundPatchesStr, NULL, foundUnlaunchInstallerVersion == v2_0 && !disableAllPatches && splashSoundBinaryPatchPath != NULL, false);
	addMenuItem(m, "Install unlaunch", NULL, foundUnlaunchInstallerVersion != INVALID && !unlaunchFound, false);
	addMenuItem(m, "Exit", NULL, true, false);

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
		messageBox("\x1B[31mError:\x1B[33m This app is exclusively for DSi.");
		return 0;
	}

	//setup sd card access
	if (!fatInitDefault())
	{
		messageBox("fatInitDefault()...\x1B[31mFailed\n\x1B[47m");
	}

	//setup nand access
	if (!fatMountSimple("nand", &io_dsi_nand))
	{
		messageBox("nand init \x1B[31mfailed\n\x1B[47m");
		return 0;
	}
	
	while (batteryLevel < 7 && !charging)
	{
		if (choiceBox("\x1B[47mBattery is too low!\nPlease plug in the console.\n\nContinue?") == NO)
			return 0;
	}
	
	DeviceList* deviceList = getDeviceList();
	
	const char* installerPath = (argc > 0) ? argv[0] : (deviceList ? deviceList->appname :  "sd:/ntrboot.nds");
	
	if (!nitroFSInit(installerPath))
	{
		messageBox("nitroFSInit()...\x1B[31mFailed\n\x1B[47m");
	}
	
	if (fileExists("sd:/unlaunch.dsi"))
	{
		foundUnlaunchInstallerVersion = loadUnlaunchInstaller("sd:/unlaunch.dsi");
		if (foundUnlaunchInstallerVersion == INVALID)
		{
			messageBox("\x1B[41mWARNING:\x1B[47m Failed to load unlaunch.dsi\n"
						"from the root of the sd card.\n"
						"Attempting to use the bundled one.");
		}
	}
	
	if(foundUnlaunchInstallerVersion == INVALID)
	{
		foundUnlaunchInstallerVersion = loadUnlaunchInstaller("nitro:/unlaunch.dsi");
		if (foundUnlaunchInstallerVersion == INVALID)
		{
			messageBox("\x1B[41mWARNING:\x1B[47m Failed to load bundled unlaunch\n"
						"installer.\n"
						"Installing unlaunch won't be possible.");
		}
	}
	
	if(fileExists("nitro:/unlaunch-patch.bin"))
	{
		splashSoundBinaryPatchPath = "nitro:/unlaunch-patch.bin";
	} else if (fileExists("sd:/unlaunch-patch.bin")) {
		splashSoundBinaryPatchPath = "sd:/unlaunch-patch.bin";
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
		
		unsigned long long tmdSize = getFileSizePath(hnaaTmdPath);
		if (tmdSize > 520)
		{
			unlaunchFound = unlaunchFound || (mainTmdIsPatched || !retailConsole);
			hnaaUnlaunchFound = true;
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
				if(unlaunchFound && nandio_unlock_writing()) {
					if(uninstallUnlaunch(retailConsole, hnaaUnlaunchFound, retailLauncherTmdPath))
					{
						messageBox("Uninstall successful!\n");
						unlaunchFound = false;
					} else {
						messageBox("\x1B[31mError:\x1B[33m Uninstall failed\n");
					}
					nandio_lock_writing();
					printf("Synchronizing FAT tables...\n");
					nandio_synchronize_fats();
				}
				break;
				
			case MAIN_MENU_CUSTOM_BG:
				if(foundUnlaunchInstallerVersion != INVALID) {
					const char* customBg = backgroundMenu();
					if(!customBg)
						break;
					if(strcmp(customBg, "default") == 0)
					{
						customBgPath = NULL;
					}
					else
					{
						customBgPath = customBg;
					}
				}
				break;
			
			case MAIN_MENU_TID_PATCHES:
				if(foundUnlaunchInstallerVersion == v1_9 || foundUnlaunchInstallerVersion == v2_0) {
					disableAllPatches = !disableAllPatches;
				}
				break;
				
			case MAIN_MENU_SOUND_SPLASH_PATCHES:
				if(foundUnlaunchInstallerVersion == v2_0 && !disableAllPatches && splashSoundBinaryPatchPath != NULL) {
					enableSoundAndSplash = !enableSoundAndSplash;
				}
				break;

			case MAIN_MENU_SAFE_UNLAUNCH_INSTALL:
				if (!unlaunchFound && foundUnlaunchInstallerVersion != INVALID && (choiceBox("Install unlaunch?") == YES)
					&& (retailLauncherTmdPresentAndToBePatched || (choiceBox("There doesn't seem to be a launcher.tmd\nfile matcing the hwinfo file\nKeep installing?") == YES))
					&& nandio_unlock_writing())
				{
					printf("Installing\n");
					if(installUnlaunch(retailConsole,
										retailLauncherTmdPresentAndToBePatched ? retailLauncherTmdPath : NULL,
										disableAllPatches,
										enableSoundAndSplash ? splashSoundBinaryPatchPath : NULL,
										customBgPath))
					{
						messageBox("Install successful!\n");
						unlaunchFound = true;
					} else {
						messageBox("\x1B[31mError:\x1B[33m Install failed\n");
					}
					nandio_lock_writing();
					printf("Synchronizing FAT tables...\n");
					nandio_synchronize_fats();
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