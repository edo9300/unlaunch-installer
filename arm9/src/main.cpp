#include <string_view>
#include <string>
#include <format>
#include <exception>
#include <memory>
#include <vector>

#include "bgMenu.h"
#include "consoleInfo.h"
#include "main.h"
#include "menu.h"
#include "message.h"
#include "nand/nandio.h"
#include "storage.h"
#include "version.h"
#include "unlaunch.h"
#include "nitrofs.h"
#include "deviceList.h"
#include "nocashFooter.h"

using namespace std::string_view_literals;

volatile bool programEnd = false;
static volatile bool arm7Exiting = false;
static bool retailLauncherTmdPresentAndToBePatched = true;
static UNLAUNCH_VERSION foundUnlaunchInstallerVersion = INVALID;
static bool disableAllPatches = false;
static bool enableSoundAndSplash = false;
static const char* splashSoundBinaryPatchPath = NULL;
static std::span<uint8_t> customBgSpan{};
volatile bool charging = false;
volatile u8 batteryLevel = 0;
static bool advancedOptionsUnlocked = false;
static bool isLauncherVersionSupported = true;

PrintConsole topScreen;
PrintConsole bottomScreen;

int bgGifTop;
int bgGifBottom;

struct Stage2 {
    Sha1Digest sha;
    bool unlaunch_supported;
};

// these are SHA1 checksums of the first block of every stage 2 known so far
// https://docs.randommeaninglesscharacters.com/stage2.html
static constexpr std::array knownStage2s{
    Stage2{"dd95fd20026925fbaaa5641517758e41397be27d"_sha1, true},  // v2435-8325_prod
    Stage2{"f546ee3cb23617b39205f6eaaa127ed347c4a132"_sha1, true},  // v2435-8325_dev
    Stage2{"8d99c1c8cf82cb672d9b3ecd587ef9eb4f4aca2d"_sha1, true},  // v2665-9336_prod
    Stage2{"7005bb39f6e2e3d2c627079b1c2d15a9b5045801"_sha1, true},  // v2725-9336_dev
    Stage2{"d734155da34c4789847b7e5fe8a7a84e131064e9"_sha1, false}, // SDMC_20080821-134255_dev
    Stage2{"70c647961d5216a3801d9b48170b294a58fe3c2e"_sha1, false}, // v1935-7470_dev
    Stage2{"fc76bd0f41f53ea8610cde197d965f5723af779a"_sha1, false}, // v1935-7470_prod
    Stage2{"4b476c6aacb5e867c025a54ad6166e423ce81293"_sha1, false}, // v2262-8067_dev
    Stage2{"d35d0870ddaf49f3db675a663c759f15dbfccd7e"_sha1, false}, // v2262-8067_prod
    Stage2{"2611443b63b94d46b4c71810d84c7b93fd5bd594"_sha1, false}, // vNONE-NONE_Unknown_dev
    Stage2{"a8b5a025378a1d0c7bbb2cf50cab6edf7b9bc312"_sha1, false}, // vNONE-NONE_Updater_dev
    Stage2{"2611443b63b94d46b4c71810d84c7b93fd5bd594"_sha1, false}, // vNONE-NONE_X4_dev
    Stage2{"0caa17616108b26f83bd98256ad0350f37504e75"_sha1, false}, // vNONE-NONE_X6_prod
};

enum {
	MAIN_MENU_SAFE_UNLAUNCH_UNINSTALL,
	MAIN_MENU_CUSTOM_BG,
	MAIN_MENU_SOUND_SPLASH_PATCHES,
	MAIN_MENU_SAFE_UNLAUNCH_INSTALL,
	MAIN_MENU_EXIT,
	MAIN_MENU_SAFE_UNLAUNCH_UNINSTALL_NO_BACKUP,
	MAIN_MENU_WRITE_NOCASH_FOOTER_ONLY,
	MAIN_MENU_TID_PATCHES,
};

static void setupScreens()
{
	videoSetMode(MODE_5_2D);
	videoSetModeSub(MODE_5_2D);

	vramSetBankA(VRAM_A_MAIN_BG);
	vramSetBankC(VRAM_C_SUB_BG);

	consoleInit(&topScreen, 1, BgType_Text4bpp, BgSize_T_256x256, 14, 0, true, true);
	consoleInit(&bottomScreen, 1, BgType_Text4bpp, BgSize_T_256x256, 14, 0, false, true);

	clearScreen(&bottomScreen);

	bgGifTop = bgInit(3, BgType_Bmp16, BgSize_B16_256x256, 2, 0);
	bgHide(bgGifTop);
	bgGifBottom = bgInitSub(3, BgType_Bmp16, BgSize_B16_256x256, 2, 0);
	bgHide(bgGifBottom);

	VRAM_A[100] = 0xFFFF;
}

static int mainMenu(const consoleInfo& info, int cursor)
{
    const auto tidPatchesSupported = (foundUnlaunchInstallerVersion == v2_0) && isLauncherVersionSupported;
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
    iprintf("\x1b[23;0Hedo9300 - 2025");

	//menu
	Menu* m = newMenu();
	setMenuHeader(m, "MAIN MENU");

    auto [restore_string, restore_string_no_backup] = [&]{
        if(info.tmdInvalid) {
            return std::make_pair("Restore launcher tmd", "Restore launcher tmd no backup");
        }
        return std::make_pair("Uninstall unlaunch", "Uninstall unlaunch no backup");
    }();

    char soundPatchesStr[64], tidPatchesStr[32], installUnlaunchStr[32];
    sprintf(tidPatchesStr, "Disable all patches: %s",
                        disableAllPatches ? "On" : "Off");
	sprintf(soundPatchesStr, "Enable sound and splash: %s",
							enableSoundAndSplash ? "On" : "Off");
	if(foundUnlaunchInstallerVersion != INVALID)
	{
		sprintf(installUnlaunchStr, "Install unlaunch (%s)", getUnlaunchVersionString(foundUnlaunchInstallerVersion));
	}
	else
	{
		strcpy(installUnlaunchStr, "Install unlaunch");
	}
    addMenuItem(m, restore_string, NULL, !info.isStockTmd() && isLauncherVersionSupported, false);
	addMenuItem(m, "Custom background", NULL, foundUnlaunchInstallerVersion != INVALID && isLauncherVersionSupported, true);
	addMenuItem(m, soundPatchesStr, NULL, foundUnlaunchInstallerVersion == v2_0 && !disableAllPatches && splashSoundBinaryPatchPath != NULL && isLauncherVersionSupported, false);
    addMenuItem(m, installUnlaunchStr, NULL, foundUnlaunchInstallerVersion != INVALID && info.isStockTmd() && isLauncherVersionSupported, false);
	addMenuItem(m, "Exit", NULL, true, false);
	if(!isLauncherVersionSupported)
    {
        addMenuItem(m, restore_string_no_backup, NULL, !info.isStockTmd(), false);
	}
	else if(advancedOptionsUnlocked)
    {
        addMenuItem(m, restore_string_no_backup, NULL, !info.isStockTmd(), false);
        addMenuItem(m, "Write nocash footer", NULL, info.needsNocashFooterToBeWritten, false);
        addMenuItem(m, tidPatchesStr, NULL, tidPatchesSupported, false);
	}

	m->cursor = cursor;

	//bottom screen
	printMenu(m);

	int konamiCode = 0;
	bool konamiCodeCooldown = false;

	while (!programEnd)
	{
		swiWaitForVBlank();
		scanKeys();

		if (moveCursor(m))
			printMenu(m);

		if (keysDown() & KEY_A)
			break;

		if(advancedOptionsUnlocked)
			continue;

		int held = keysHeld();

		if ((held & (KEY_L | KEY_R | KEY_Y)) == (KEY_L | KEY_R | KEY_Y))
		{
			if(held == (KEY_L | KEY_R | KEY_Y) && !konamiCodeCooldown)
			{
				konamiCodeCooldown = true;
				++konamiCode;
			}
		}
		else
		{
			konamiCodeCooldown = false;
		}
		if (konamiCode == 5)
		{
			advancedOptionsUnlocked = true;
			// Enabled by default when unsupported
			if(isLauncherVersionSupported)
			{
                addMenuItem(m, "Uninstall unlaunch no backup", NULL, !info.isStockTmd(), false);
			}
            addMenuItem(m, "Write nocash footer", NULL, info.needsNocashFooterToBeWritten, false);
            addMenuItem(m, tidPatchesStr, NULL, tidPatchesSupported, false);
		}
	}

	int result = m->cursor;
	freeMenu(m);

	return result;
}

void setup() {
    keysSetRepeat(25, 5);
    setupScreens();

    fifoSetValue32Handler(FIFO_USER_01, [](u32 value32, void*) {
        if (value32 != 0x54495845) // 'EXIT'
            return;
        programEnd = true;
        arm7Exiting = true;
    }, NULL);
    fifoSetValue32Handler(FIFO_USER_03, [](u32 value32, void*) {
        batteryLevel = value32 & 0xF;
        charging = (value32 & BIT(7)) != 0;
    }, NULL);

    //DSi check
    if (!isDSiMode())
    {
        messageBox("\x1B[31mError:\x1B[33m This app is exclusively for DSi.");
        exit(0);
    }

    //setup sd card access
    if (!fatInitDefault())
    {
        messageBox("fatInitDefault()...\x1B[31mFailed\n\x1B[47m");
    }

    u32 clusterSize = getClusterSizeForPartition("sd:/");
    if(clusterSize > 32768)
    {
        messageBox(std::format("\x1B[41mWARNING:\x1B[47m This SD card cluster\n"
                               "size is currently {}KB,\n"
                               "which is too large for Unlaunch\n"
                               "to work.\n"
                               "If you install it, Unlaunch\n"
                               "won't boot as long as this SD\n"
                               "card is inserted.", clusterSize / 1024).data());
    }

    //setup nand access
    if (!fatMountSimple("nand", &io_dsi_nand))
    {
        messageBox("nand init \x1B[31mfailed\n\x1B[47m");
        exit(0);
    }
}

void checkStage2Supported() {
    Sha1Digest digest;
    nandio_calculate_stage2_sha(digest.data());
    for(const auto& [sha, unlaunch]: knownStage2s) {
        if(sha == digest) {
            if(!unlaunch) {
                messageBox("\x1B[31mError:\x1B[33m A known stage2 was found but is not compatible with\n"
                           "unlaunch.");
                exit(0);
            }
            return;
        }
    }
    messageBox("\x1B[31mError:\x1B[33m An unknown stage2\n"
               "was found. This is a rare find,\n"
               "you should look for help\n"
               "archiving and documenting\n"
               "your nand");
    exit(0);
}

std::vector<std::string_view> getInstallerPaths(int argc, char **argv) {
	if(argc > 0)
		return {std::string_view{argv[0]}};
	DeviceList* deviceList = getDeviceList();

	if(deviceList) return {std::string_view{ deviceList->appname}};

	return {"sd:/ntrboot.nds", "sd:/boot.nds"};
}

void setupNitrofs(int argc, char **argv) {
	for(const auto& path : getInstallerPaths(argc, argv)){
		if(!nitroFSInit(path.data()))
			continue;
		auto* file = fopen("nitro:/installer.ver", "rb");
		if(!file)
			continue;
		char verstring[10]{};
		fread(verstring, 1, sizeof(verstring) - 1, file);
		fclose(file);
		if(std::string_view{verstring} == VERSION)
			return;
	}
	messageBox("nitroFSInit()...\x1B[31mFailed\n\x1B[47m");
	exit(0);
}

void checkNocashFooter(consoleInfo& info) {
    NocashFooter footer;

    nandio_read_nocash_footer(&footer);
    nandio_construct_nocash_footer(&info.nocashFooter);

    info.needsNocashFooterToBeWritten = !isFooterValid(&footer);

    if(!info.needsNocashFooterToBeWritten)
    {
        if(memcmp(&footer, &info.nocashFooter, sizeof(footer)) != 0)
        {
            messageBox("\x1B[31mError:\x1B[33m This console has a\n"
                       "nocash footer embedded in its\n"
                       "nand that doesn't match the one\n"
                       "generated.\n"
                       "The footer already present will\n"
                       "be overwritten.");
            info.needsNocashFooterToBeWritten = true;
        }
    }
}

bool writeNocashFooter(consoleInfo& info) {
    if(!info.needsNocashFooterToBeWritten)
        return true;

    if(!nandio_unlock_writing())
        return false;

    printf("Writing nocash footer\n");
    auto res = nandio_write_nocash_footer(&info.nocashFooter);
    nandio_lock_writing();
    if(!res)
    {
        messageBox("Failed to write nocash footer");
        return false;
    }
    info.needsNocashFooterToBeWritten = false;
    return true;
}

void waitForBatteryChargedEnough() {

    while (batteryLevel < 7 && !charging)
    {
        if (choiceBox("\x1B[47mBattery is too low!\nPlease plug in the console.\n\nContinue?") == NO)
            exit(0);
    }
}

void loadUnlaunchInstaller() {
    if (fileExists("sd:/unlaunch.dsi"))
    {
        foundUnlaunchInstallerVersion = loadUnlaunchInstaller("sd:/unlaunch.dsi");
        if(foundUnlaunchInstallerVersion != INVALID)
            return;

        messageBox("\x1B[41mWARNING:\x1B[47m Failed to load unlaunch.dsi\n"
                   "from the root of the sd card.\n"
                   "Attempting to use the bundled one.");
    }

    foundUnlaunchInstallerVersion = loadUnlaunchInstaller("nitro:/unlaunch.dsi");

    if(foundUnlaunchInstallerVersion != INVALID)
        return;

    messageBox("\x1B[41mWARNING:\x1B[47m Failed to load bundled unlaunch\n"
               "installer.\n"
               "Installing unlaunch won't be possible.");
}

void loadUnlaunchInstallerPatch() {
    if (fileExists("sd:/sound-and-splash-patch.bin")) {
        splashSoundBinaryPatchPath = "sd:/sound-and-splash-patch.bin";
    } else if(fileExists("nitro:/sound-and-splash-patch.bin")) {
        splashSoundBinaryPatchPath = "nitro:/sound-and-splash-patch.bin";
    }
    if(!fileExists("nitro:/force-hnaa-patch.bin")) {
        throw std::runtime_error(std::format("Failed to find hnaa patch ({})", "nitro:/force-hnaa-patch.bin"));
    }
}

void parseLauncherInfo(std::string_view launcher_tid_str, consoleInfo& info) {
    auto launcher_content_path = std::format("nand:/title/00030017/{}/content", launcher_tid_str);

    auto [tmd_found, expected_launcher_build, retailLauncherPath] = [&] {
        std::shared_ptr<DIR> pdir{opendir(launcher_content_path.c_str()), closedir};
        if (!pdir)
            throw std::runtime_error(std::format("Could not open launcher title directory ({})", launcher_content_path));
        dirent* pent;
        std::optional<std::pair<uint32_t, std::string>> foundApp;
        bool tmdFound;
        while((pent = readdir(pdir.get())) != nullptr) {
            if(foundApp && tmdFound) {
                break;
            }
            if(pent->d_type == DT_DIR)
                continue;
            std::string_view filename{pent->d_name};
            if(filename == "title.tmd") {
                tmdFound = true;
                continue;
            }

            if(filename.size() != 12 || !filename.ends_with(".app") || !filename.starts_with("0000000"))
                continue;

            auto launcher_app_version = static_cast<uint16_t>(static_cast<unsigned char>(filename[7]) - static_cast<unsigned char>('0'));
            if(launcher_app_version > 7)
                throw std::runtime_error(std::format("Found an unsupported launcher version: {}", launcher_app_version));

            foundApp = std::make_pair(static_cast<uint32_t>(256 * launcher_app_version), std::string{filename});
        }
        if(!foundApp)
            throw std::runtime_error("Launcher app not found");
        const auto& [launcher_build, launcher_app_name] = *foundApp;
        return std::make_tuple(tmdFound, launcher_build, std::format("{}/{}", launcher_content_path, launcher_app_name));
    }();

    if((info.tmdFound = tmd_found)) {
        const auto recoveryTmdPath = std::format("nitro:/{}/tmd.{}", launcher_tid_str, static_cast<int>(expected_launcher_build));
        info.launcherTmdPath = std::format("{}/title.tmd", launcher_content_path);
        info.recoveryTmdDataSha = [&] -> Sha1Digest {
            auto file = fopen(std::format("{}.sha1", recoveryTmdPath).data(), "rb");
            if(!file)
                throw std::runtime_error("Good tmd sha1 not found");
            char sha1StrBuff[41]{};
            auto read = fread(sha1StrBuff, sizeof(sha1StrBuff) - 1, 1, file);
            fclose(file);
            if(read != 1)
                throw std::runtime_error("Failed to parse good tmd's sha1 file");
            return {sha1StrBuff};
        }();

        auto patchedTmdSha1 = [&] -> Sha1Digest {
            auto file = fopen(std::format("{}.patch.sha1", recoveryTmdPath).data(), "rb");
            if(!file)
                throw std::runtime_error("Patched tmd sha1 not found");
            char sha1StrBuff[41]{};
            auto read = fread(sha1StrBuff, sizeof(sha1StrBuff) - 1, 1, file);
            fclose(file);
            if(read != 1)
                throw std::runtime_error("Failed to parse patched tmd's sha1 file");
            return {sha1StrBuff};
        }();

        info.recoveryTmdData = [&] {
            auto* sourceTmd = fopen(recoveryTmdPath.data(), "rb");

            std::array<uint8_t, 520> ret;
            auto read = fread(ret.data(), ret.size(), 1, sourceTmd);
            fclose(sourceTmd);
            if(read != 1)
            {
                throw std::runtime_error("Failed to read good tmd's buffer");
            }

            Sha1Digest digest;
            swiSHA1Calc(digest.data(), ret.data(), ret.size());
            if(digest != info.recoveryTmdDataSha)
            {
                throw std::runtime_error("Good tmd's sha mismatching");
            }
            return ret;
        }();

		std::shared_ptr<FILE> tmd{fopen(info.launcherTmdPath.data(), "rb"), [](auto* ptr){ if(ptr) fclose(ptr);}};
		if(!tmd) {
            info.tmdFound = false;
		} else if(auto tmdSize = getFileSize(tmd.get()); tmdSize < 520) {
            //if size isn't at least 520 then the tmd is already invalid
            info.tmdInvalid = true;
        } else {
            info.launcherAppPath = retailLauncherPath;
            if(tmdSize > 520) {
                info.tmdInvalid = true;
            }
            else
            {
                Sha1Digest digest;
                calculateFileSha1(tmd.get(), &digest);
                if(digest == info.recoveryTmdDataSha){
                    info.tmdGood = true;
                } else if(digest == patchedTmdSha1) {
                    info.tmdPatched = true;
                } else {
                    info.tmdInvalid = true;
                }
            }
            if(!info.tmdInvalid) {
                fseek(tmd.get(), 0x1DC, SEEK_SET);
                uint16_t launcherVersion;
                fread(&launcherVersion, sizeof(launcherVersion), 1, tmd.get());
                if(static_cast<uint32_t>(launcherVersion) * 256 != expected_launcher_build) {
                    throw std::runtime_error("Launcher version found doesn't match with the one in the tmd");
                }
                info.launcherVersion = launcherVersion;
            }
        }
        if(info.tmdInvalid || !info.tmdFound) {
            // if the tmd is invalid, don't read the launcher version from it and assume it's the one
            // matching the app file
            info.launcherVersion = expected_launcher_build / 256;
        }
    }
}

void retrieveInstalledLauncherInfo(consoleInfo& info) {
    static constexpr auto hnaaTmdPath = "nand:/title/00030017/484e4141/content/title.tmd"sv;
    const auto [launcher_tid_str, region] = [] -> std::pair<std::string, u8> {
        uint32_t launcherTid;
        {
            auto* file = fopen("nand:/sys/HWINFO_S.dat", "rb");
            if(!file)
                return std::make_pair("", static_cast<u8>(0xFF));
            fseek(file, 0xA0, SEEK_SET);
            fread(&launcherTid, sizeof(uint32_t), 1, file);
            fclose(file);
        }
        return std::make_pair(std::format("{:08x}", launcherTid), static_cast<u8>(launcherTid & 0xFF));
    }();

    // I own and know of many people with retail and dev prototypes
    // These can normally be identified by having the region set to ALL (0x41)
    info.isRetail = (region != 0x41 && region != 0xFF);

    //check for unlaunch and region
    if (info.isRetail && launcher_tid_str.size() != 0) {
        parseLauncherInfo(launcher_tid_str, info);
    } else {
        // HWINFO_S may not always exist (PRE_IMPORT). Fill in defaults if that happens.
        (void)0;
    }

    if (auto tmdSize = getFileSizePath(hnaaTmdPath.data()); tmdSize > 520) {
        info.UnlaunchHNAAtmdFound = true;
    }
}

void uninstall(consoleInfo& info, bool noBackup) {
    if(info.isStockTmd())
    {
        return;
    }
    bool unsafeUninstall = advancedOptionsUnlocked && noBackup;
    if(!isLauncherVersionSupported && !unsafeUninstall)
    {
        return;
    }
    printf("Uninstalling");
    if(!writeNocashFooter(info))
    {
        return;
    }
    if(!nandio_unlock_writing())
    {
        return;
    }
    if(uninstallUnlaunch(info, unsafeUninstall))
    {
        messageBox("Uninstall successful!\n");
        info.tmdInvalid = false;
        info.tmdPatched = false;
        info.tmdGood = true;
        info.UnlaunchHNAAtmdFound = !unsafeUninstall;
    }
    else
    {
        messageBox("\x1B[31mError:\x1B[33m Uninstall failed\n");
    }
    nandio_lock_writing();
    printf("Synchronizing FAT tables...\n");
    nandio_synchronize_fats();
}

void install(consoleInfo& info) {
    if(!isLauncherVersionSupported)
    {
        return;
    }
    if(!info.isStockTmd())
    {
        return;
    }
    if(foundUnlaunchInstallerVersion == INVALID)
    {
        return;
    }
    if(choiceBox("Install unlaunch?") == NO)
    {
        return;
    }
    if(!retailLauncherTmdPresentAndToBePatched
        && (choiceBox("There doesn't seem to be a launcher.tmd\n"
                      "file matcing the hwinfo file\n"
                      "Keep installing?") == NO))
    {
        return;
    }
    printf("Installing\n");
    if(!writeNocashFooter(info))
    {
        return;
    }
    if(!nandio_unlock_writing())
    {
        return;
    }
    if(installUnlaunch(info, disableAllPatches,
                        enableSoundAndSplash ? splashSoundBinaryPatchPath : NULL,
                        customBgSpan))
    {
        messageBox("Install successful!\n");
        info.tmdGood = false;
        info.tmdPatched = true;
        info.UnlaunchHNAAtmdFound = true;
    }
    else
    {
        messageBox("\x1B[31mError:\x1B[33m Install failed\n");
    }
    nandio_lock_writing();
    printf("Synchronizing FAT tables...\n");
    nandio_synchronize_fats();
}

void customBg() {
    if(!isLauncherVersionSupported)
    {
        return;
    }
    if(foundUnlaunchInstallerVersion == INVALID)
    {
        return;
    }
	if(auto newBg = backgroundMenu(); newBg.has_value())
		customBgSpan = *newBg;
}

void doMainMenu(consoleInfo& info) {
    int cursor = 0;
    while(!programEnd)
    {
        cursor = mainMenu(info, cursor);
        if(programEnd)
            break;

        switch (cursor)
        {
        case MAIN_MENU_SAFE_UNLAUNCH_UNINSTALL:
        case MAIN_MENU_SAFE_UNLAUNCH_UNINSTALL_NO_BACKUP:
        {
            uninstall(info, cursor == MAIN_MENU_SAFE_UNLAUNCH_UNINSTALL_NO_BACKUP);
        }
        break;

        case MAIN_MENU_CUSTOM_BG:
        {
            customBg();
        }
        break;

        case MAIN_MENU_TID_PATCHES:
            if(!isLauncherVersionSupported)
            {
                break;
            }
            if(advancedOptionsUnlocked && (foundUnlaunchInstallerVersion == v2_0)) {
                disableAllPatches = !disableAllPatches;
            }
            break;

        case MAIN_MENU_SOUND_SPLASH_PATCHES:
            if(!isLauncherVersionSupported)
            {
                break;
            }
            if(foundUnlaunchInstallerVersion == v2_0 && !disableAllPatches && splashSoundBinaryPatchPath != NULL) {
                enableSoundAndSplash = !enableSoundAndSplash;
            }
            break;

        case MAIN_MENU_SAFE_UNLAUNCH_INSTALL:
        {
            install(info);
        }
        break;

        case MAIN_MENU_WRITE_NOCASH_FOOTER_ONLY:
            (void)writeNocashFooter(info);
            break;

        case MAIN_MENU_EXIT:
            programEnd = true;
            return;
        }
    }
}

int main(int argc, char **argv)
{
    setup();
    checkStage2Supported();
	setupNitrofs(argc, argv);

    loadUnlaunchInstaller();

    try {
        loadUnlaunchInstallerPatch();

        consoleInfo info;

        retrieveInstalledLauncherInfo(info);

        checkNocashFooter(info);

        // Launcher v4, build v1024 (shipped with firmware 1.4.2 (1.4.3 for china and korea)
        // will fail to launch if another tmd withouth appropriate application, or an invalid
        // tmd (in our case the one installed from unlaunch) is found in the HNAA launcher folder
        // there's really no workaround to that, so that specific version is blacklisted and only uninstalling
        // an "officially" installed unlaunch without leaving any backup behind will be allowed
        if(info.launcherVersion == 4) {
            isLauncherVersionSupported = false;
            messageBox("\x1B[41mWARNING:\x1B[47m This system version\n"
                       "doesn't support this install\n"
                       "method, only uninstalling\n"
                       "unaunch without backups will\n"
                       "be possible");
        }

        messageBox("\x1B[41mWARNING:\x1B[47m This tool can write to\n"
                   "your internal NAND!\n\n"
                   "This always has a risk, albeit\n"
                   "low, of \x1B[41mbricking\x1B[47m your system\n"
                   "and should be done with caution!\n\n"
                   "If you have not yet done so,\n"
                   "you should make a NAND backup.");

        waitForBatteryChargedEnough();

        doMainMenu(info);
    } catch (const std::exception& e) {
        messageBox(e.what());
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
