#include "main.h"
#include "menu.h"
#include "message.h"
#include "nand/nandio.h"
#include "storage.h"
#include "version.h"
#include <errno.h>
#include <dirent.h>
#include <time.h>

bool programEnd = false;
bool sdnandMode = true;
bool hasTitleTmdMatchingLauncher = true;
bool unlaunchInstallerFound = false;
bool unlaunchFound = false;
bool unlaunchPatches = false;
bool devkpFound = false;
bool launcherDSiFound = false;
bool arm7Exiting = false;
bool charging = false;
u8 batteryLevel = 0;
u8 region = 0;

PrintConsole topScreen;
PrintConsole bottomScreen;

enum {
	MAIN_MENU_MODE,
	MAIN_MENU_INSTALL,
	MAIN_MENU_SAFE_UNLAUNCH_UNINSTALL,
	MAIN_MENU_SAFE_UNLAUNCH_INSTALL,
	MAIN_MENU_TITLES,
	MAIN_MENU_BACKUP,
	MAIN_MENU_TEST,
	MAIN_MENU_FIX,
	MAIN_MENU_DATA_MANAGEMENT,
	MAIN_MENU_LANGUAGE_PATCHER,
	MAIN_MENU_EXIT
};

static void _setupScreens()
{
	REG_DISPCNT = MODE_FB0;
	VRAM_A_CR = VRAM_ENABLE;

	videoSetMode(MODE_0_2D);
	videoSetModeSub(MODE_0_2D);

	vramSetBankA(VRAM_A_MAIN_BG);
	vramSetBankC(VRAM_C_SUB_BG);

	consoleInit(&topScreen,    3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true,  true);
	consoleInit(&bottomScreen, 3, BgType_Text4bpp, BgSize_T_256x256, 31, 0, false, true);

	clearScreen(&bottomScreen);

	VRAM_A[100] = 0xFFFF;
}

static int _mainMenu(int cursor)
{
	//top screen
	clearScreen(&topScreen);

	iprintf("\t\tNAND Title Manager\n");
	iprintf("\t\t\tmodified from\n");
	iprintf("\tTitle Manager for HiyaCFW\n");
	iprintf("\nversion %s\n", VERSION);
	iprintf("\n\n\x1B[41mWARNING:\x1B[47m This tool can write to\nyour internal NAND!\n\nThis always has a risk, albeit\nlow, of \x1B[41mbricking\x1B[47m your system\nand should be done with caution!\n");
	iprintf("\n\t  \x1B[46mhttps://dsi.cfw.guide\x1B[47m\n");
	iprintf("\n\n \x1B[46mgithub.com/Epicpkmn11/NTM/wiki\x1B[47m\n");
	iprintf("\x1b[22;0HJeff - 2018-2019");
	iprintf("\x1b[23;0HPk11 - 2022-2023");

	//menu
	Menu* m = newMenu();
	setMenuHeader(m, "MAIN MENU");

	char modeStr[32], datamanStr[32], launcherStr[32];
	sprintf(modeStr, "Mode: %s", sdnandMode ? "SDNAND" : "\x1B[41mSysNAND\x1B[47m");
	sprintf(datamanStr, "\x1B[%02omEnable Data Management", devkpFound ? 037 : 047);
	sprintf(launcherStr, "\x1B[%02omUninstall region mod", launcherDSiFound ? 047 : 037);
	addMenuItem(m, modeStr, NULL, 0);
	addMenuItem(m, "Install", NULL, 0);
	addMenuItem(m, "Safe unlaunch uninstall", NULL, 0);
	addMenuItem(m, "Safe unlaunch install", NULL, 0);
	addMenuItem(m, "Titles", NULL, 0);
	addMenuItem(m, "Restore", NULL, 0);
	addMenuItem(m, "Test", NULL, 0);
	addMenuItem(m, "Fix FAT copy mismatch", NULL, 0);
	addMenuItem(m, datamanStr, NULL, 0);
	addMenuItem(m, launcherStr, NULL, 0);
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

void fifoHandlerPower(u32 value32, void* userdata)
{
	if (value32 == 0x54495845) // 'EXIT'
	{
		programEnd = true;
		arm7Exiting = true;
	}
}

void fifoHandlerBattery(u32 value32, void* userdata)
{
	batteryLevel = value32 & 0xF;
	charging = (value32 & BIT(7)) != 0;
}

bool checkIfUnlaunchHasPatches(const char* path)
{
	//check if launcher patches are enabled
	const static u32 tidValues[][2] = {
		// {location, value}
		{0xE439, 0x382E3176}, // 1.8
		{0xB07C, 0x17484E41}, // 1.9
		{0xB099, 0x17484E41}, // 2.0 (Normal)
		{0xB079, 0x484E1841}, // 2.0 (Patched)
	};
	
	bool patched = false;

	FILE *tmd = fopen(path, "rb");
	if (tmd)
	{
		for (int i = 0; i < sizeof(tidValues) / sizeof(tidValues[0]); i++)
		{
			if (fseek(tmd, tidValues[i][0], SEEK_SET) == 0)
			{
				u32 tidVal;
				fread(&tidVal, sizeof(u32), 1, tmd);
				if (tidVal == tidValues[i][1])
				{
					patched = true;
					break;
				}
			}
		}
	}
	fclose(tmd);
	return patched;
}

bool safeCreateDir(const char* path)
{
	if (((mkdir(path, 0777) == 0) || errno == EEXIST))
		return true;
	
	char errorStr[512];
	sprintf(errorStr, "\x1B[31mError:\x1B[33m Failed to create directory (%s)\n", path);
	
	messageBox(errorStr);
	return false;
}

int main(int argc, char **argv)
{
	srand(time(0));
	keysSetRepeat(25, 5);
	_setupScreens();

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
	
	unlaunchInstallerFound = (access("sd:/unlaunch.dsi", F_OK) == 0);

	//check for unlaunch and region
	char launcherTmdPath[64];
	{
		FILE *file = fopen("nand:/sys/HWINFO_S.dat", "rb");
		if (file)
		{
			fseek(file, 0xA0, SEEK_SET);
			u32 launcherTid;
			fread(&launcherTid, sizeof(u32), 1, file);
			fclose(file);

			region = launcherTid & 0xFF;

			sprintf(launcherTmdPath, "nand:/title/00030017/%08lx/content/title.tmd", launcherTid);
			unsigned long long tmdSize = getFileSizePath(launcherTmdPath);
			if(tmdSize != 520)
			{
				hasTitleTmdMatchingLauncher = false;
			}
			else if (tmdSize > 520)
			{
				unlaunchFound = true;
				unlaunchPatches = checkIfUnlaunchHasPatches(launcherTmdPath);
			}
		}
		
		if (!unlaunchFound)
		{
			unsigned long long tmdSize = getFileSizePath("nand:/title/00030017/484e4141/content/title.tmd");
			if (tmdSize > 520)
			{
				unlaunchFound = true;
				unlaunchPatches = checkIfUnlaunchHasPatches("nand:/title/00030017/484e4141/content/title.tmd");
			}
		}

		if (!unlaunchFound)
		{
			messageBox("Unlaunch not found. TMD files\nwill be required and there\nis a greater risk something\ncould go wrong.\n\nSee \x1B[46mhttps://dsi.cfw.guide/\x1B[47m to\ninstall.");
		}
		else if (!unlaunchPatches)
		{
			messageBox("Unlaunch's Launcher Patches are\nnot enabled. You will need to\nprovide TMD files or reinstall.\n\n\x1B[46mhttps://dsi.cfw.guide/\x1B[47m");
		}
	}

	//check for dev.kp (Data Management visible)
	devkpFound = (access("sd:/sys/dev.kp", F_OK) == 0);

	//check for launcher.dsi (Language patcher)
	launcherDSiFound = (access("nand:/launcher.dsi", F_OK) == 0);

	messageBox("\x1B[41mWARNING:\x1B[47m This tool can write to\nyour internal NAND!\n\nThis always has a risk, albeit\nlow, of \x1B[41mbricking\x1B[47m your system\nand should be done with caution!\n\nIf you have not yet done so,\nyou should make a NAND backup.");

	messageBox("If you are following a video\nguide, please stop.\n\nVideo guides for console moddingare often outdated or straight\nup incorrect to begin with.\n\nThe recommended guide for\nmodding your DSi is:\n\n\x1B[46mhttps://dsi.cfw.guide/\x1B[47m\n\nFor more information on using\nNTM, see the official wiki:\n\n\x1B[46mhttps://github.com/Epicpkmn11/\n\t\t\t\t\t\t\t\tNTM/wiki\x1B[47m");
	//main menu
	int cursor = 0;

	while (!programEnd)
	{
		cursor = _mainMenu(cursor);

		switch (cursor)
		{
			case MAIN_MENU_MODE:
				sdnandMode = !sdnandMode;
				devkpFound = (access(sdnandMode ? "sd:/sys/dev.kp" : "nand:/sys/dev.kp", F_OK) == 0);
				break;

			case MAIN_MENU_INSTALL:
				installMenu();
				break;

			case MAIN_MENU_SAFE_UNLAUNCH_INSTALL:
				if (unlaunchInstallerFound && (choiceBox("Install unlaunch?") == YES)
					&& (hasTitleTmdMatchingLauncher || (choiceBox("There doesn't seem to be a launcher.tmd\nfile matcing the hwinfo file\nKeep installing?") == YES))
					&& nandio_unlock_writing())
				{
					
					FILE* unlaunchInstaller = fopen("sd:/unlaunch.dsi", "rb");
					if (!unlaunchInstaller)
					{
						messageBox("\x1B[31mError:\x1B[33m Failed to open unlaunch installer\n");
						nandio_lock_writing();
						break;
					}
					//Create HNAA launcher folder
					if (!safeCreateDir("nand:/title/00030017")
						|| !safeCreateDir("nand:/title/00030017/484e4141")
						|| !safeCreateDir("nand:/title/00030017/484e4141/content")) {
						nandio_lock_writing();
						break;
					}

					FILE* targetTmd = fopen("nand:/title/00030017/484e4141/content/title.tmd", "wb");
					if (!targetTmd)
					{
						fclose(unlaunchInstaller);
						messageBox("\x1B[31mError:\x1B[33m Failed to open target unlaunch tmd\n");
						rmdir("nand:/title/00030017/484e4141/content");
						rmdir("nand:/title/00030017/484e4141");
						nandio_lock_writing();
						break;
					}
					
					{
						char buffer[512] = {0};
						//write the first 512 bytes as 0, as that's the size of a tmd, but it can be whatever						
						if (fwrite(buffer, sizeof(char), 512, targetTmd) != 512)
						{
							fclose(unlaunchInstaller);
							fclose(targetTmd);
							messageBox("\x1B[31mError:\x1B[33m Failed write to target unlaunch tmd\n");
							remove("nand:/title/00030017/484e4141/content/title.tmd");
							rmdir("nand:/title/00030017/484e4141/content");
							rmdir("nand:/title/00030017/484e4141");
							nandio_lock_writing();
							break;
						}
						
						size_t n;
						bool failed = false;

						while ((n = fread(buffer, sizeof(char), sizeof(buffer), unlaunchInstaller)) > 0)
						{
							if (fwrite(buffer, sizeof(char), n, targetTmd) != n)
							{
								fclose(unlaunchInstaller);
								fclose(targetTmd);
								messageBox("\x1B[31mError:\x1B[33m Failed write to target unlaunch tmd\n");
								remove("nand:/title/00030017/484e4141/content/title.tmd");
								rmdir("nand:/title/00030017/484e4141/content");
								rmdir("nand:/title/00030017/484e4141");
								nandio_lock_writing();
								failed = true;
								break;
							}
						}
						if (failed)
							break;
						if (!feof(unlaunchInstaller) || ferror(unlaunchInstaller))
						{
							fclose(unlaunchInstaller);
							fclose(targetTmd);
							messageBox("\x1B[31mError:\x1B[33m Failed read unlaunch installer\n");
							remove("nand:/title/00030017/484e4141/content/title.tmd");
							rmdir("nand:/title/00030017/484e4141/content");
							rmdir("nand:/title/00030017/484e4141");
							nandio_lock_writing();
							break;
						}
					}				
					fclose(unlaunchInstaller);	
					fclose(targetTmd);

					//Mark the tmd as readonly
					int fatAttributes = FAT_getAttr("nand:/title/00030017/484e4141/content/title.tmd");
					if(!FAT_setAttr("nand:/title/00030017/484e4141/content/title.tmd", fatAttributes | ATTR_READONLY) != 0)
					{
						messageBox("\x1B[31mError:\x1B[33m Failed to mark unlaunch's title.tmd as read only\n");
						remove("nand:/title/00030017/484e4141/content/title.tmd");
						rmdir("nand:/title/00030017/484e4141/content");
						rmdir("nand:/title/00030017/484e4141");
						nandio_lock_writing();
						break;
					}

					//Finally patch the default launcher tmd to be invalid

					//If there isn't a title.tmd matching the language region in the hwinfo
					// nothing else has to be done, could be a language patch, or a dev system, the user will know what they have done
					if (hasTitleTmdMatchingLauncher)
					{
						FILE* launcherTmd = fopen(launcherTmdPath, "rb");
						if(!launcherTmd)
						{
							messageBox("\x1B[31mError:\x1B[33m Failed to open default launcher's title.tmd\n");
							remove("nand:/title/00030017/484e4141/content/title.tmd");
							rmdir("nand:/title/00030017/484e4141/content");
							rmdir("nand:/title/00030017/484e4141");
							nandio_lock_writing();
							break;
						}
						FILE * f = fopen("title.tmd", "r+b");
						// Patches the title.tmd's title id from HNXX to GNXX
						fseek(launcherTmd, 0x190, SEEK_SET);
						char c;
						fread(&c, 1, 1, launcherTmd);
						//if byte is not already 
						if(c == 0x48)
						{
							fseek(launcherTmd, -1, SEEK_CUR);
							c = 0x47;
							fwrite(&launcherTmd, 1, 1, f);
						}
						else if(c != 0x47)
						{
							messageBox("\x1B[31mError:\x1B[33m Default launcher's title.tmd was tamprered with, aborting\n");
							remove("nand:/title/00030017/484e4141/content/title.tmd");
							rmdir("nand:/title/00030017/484e4141/content");
							rmdir("nand:/title/00030017/484e4141");
							nandio_lock_writing();
							fclose(launcherTmd);
							break;
						}
						fclose(launcherTmd);
					}
					nandio_lock_writing();
					unlaunchFound = true;
					messageBox("Unlaunch has been installed.\n");
				}
				break;

			case MAIN_MENU_TITLES:
				titleMenu();
				break;

			case MAIN_MENU_BACKUP:
				backupMenu();
				break;

			case MAIN_MENU_TEST:
				testMenu();
				break;

			case MAIN_MENU_FIX:
				if (nandio_unlock_writing())
				{
					nandio_force_fat_fix();
					nandio_lock_writing();
					messageBox("Mismatch in FAT copies will be\nfixed on close.\n");
				}
				break;

			case MAIN_MENU_DATA_MANAGEMENT:
				if (!devkpFound && (choiceBox("Make Data Management visible\nin System Settings?") == YES) && (sdnandMode || nandio_unlock_writing()))
				{
					//ensure sys folder exists
					if(access(sdnandMode ? "sd:/sys" : "nand:/sys", F_OK) != 0)
						mkdir(sdnandMode ? "sd:/sys" : "nand:/sys", 0777);

					//create empty file
					FILE *file = fopen(sdnandMode ? "sd:/sys/dev.kp" : "nand:/sys/dev.kp", "wb");
					fclose(file);

					if(!sdnandMode)
						nandio_lock_writing();
					devkpFound = (access(sdnandMode ? "sd:/sys/dev.kp" : "nand:/sys/dev.kp", F_OK) == 0);
					messageBox("Data Management is now visible\nin System Settings.\n");
				}
				break;
			case MAIN_MENU_LANGUAGE_PATCHER:
				if (launcherDSiFound && (choiceBox("Uninstall the language patched\nDSi Menu? (launcher.dsi)") == YES) && nandio_unlock_writing())
				{
					//delete launcher.dsi
					remove("nand:/launcher.dsi");

					nandio_lock_writing();
					launcherDSiFound = (access("nand:/launcher.dsi", F_OK) == 0);
					messageBox("The language patched DSi Menu\nhas been removed.\n");
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