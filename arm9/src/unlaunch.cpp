#include "message.h"
#include "sha1digest.h"
#include "storage.h"
#include "unlaunch.h"
#include <algorithm>
#include <array>
#include <nds/sha1.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

static char unlaunchInstallerBuffer[0x30000];
static const char* hnaaTmdPath = "nand:/title/00030017/484e4141/content/title.tmd";
static const char* hnaaBackupTmdPath = "nand:/title/00030017/484e4141/content/title.tmd.bak";

enum UNLAUNCH_VERSION {
	v1_8,
	v1_9,
	v2_0,
	INVALID,
};

UNLAUNCH_VERSION installerVersion{INVALID};
size_t unlaunchInstallerSize{};

constexpr std::array knownUnlaunchHashes{
	/*"9e6a8d95062533dfc422362f99ff3e24e7de9920"_sha1, // v0.8: blacklisted, doesn't like this install method*/
	/*"fb0d0ffebda67b786f608bf5cbcb2efee6ab42bb"_sha1, // v0.9: blacklisted, doesn't like this install method*/
	/*"d710ff585e321082b33456dd4e0568200c9adcc7"_sha1, // v1.0: blacklisted, doesn't like this install method*/
	/*"4f3e455e0a752d35a219a3ff10ba14a6c98bff13"_sha1, // v1.1: blacklisted, doesn't like this install method*/
	/*"25db1a47ba84748f911d9f4357bfc417533121c7"_sha1, // v1.2: blacklisted, doesn't like this install method*/
	/*"068f1d56da02bb4f93fb76d7874e14010c7e7a3d"_sha1, // v1.3: blacklisted, doesn't like this install method*/
	/*"43197370de74d302ef7c4420059c3ca7d50c4f3d"_sha1, // v1.4: blacklisted, doesn't like this install method*/
	/*"0525b28cc59b6f7fc00ad592aebadd7257bf7efb"_sha1, // v1.5: blacklisted, doesn't like this install method*/
	/*"9470a51fde188235052b119f6bfabf6689cb2343"_sha1, // v1.6: blacklisted, doesn't like this install method*/
	/*"672c11eb535b97b0d32ff580d314a2ad6411d5fe"_sha1, // v1.7: blacklisted, doesn't like this install method*/
	"b76c2b1722e769c6c0b4b3d4bc73250e41993229"_sha1, // v1.8
	"f3eb41cba136a3477523155f8b05df14917c55f4"_sha1, // v1.9
	"15f4a36251d1408d71114019b2825fe8f5b4c8cc"_sha1, // v2.0
};

bool isValidUnlaunchInstallerSize(size_t size)
{
	return size == 163320 /*1.8*/ || size == 196088 /*1.9*/;
}

bool isLauncherTmdPatched(const char* path)
{
	FILE* launcherTmd = fopen(path, "rb");
	if(!launcherTmd)
	{
		return false;
	}
	fseek(launcherTmd, 0x190, SEEK_SET);
	char c;
	fread(&c, 1, 1, launcherTmd);
	fclose(launcherTmd);
	return c == 0x47;
}

static bool restoreMainTmd(const char* path)
{
	FILE* launcherTmd = fopen(path, "r+b");
	if(!launcherTmd)
	{
		messageBox("\x1B[31mError:\x1B[33m Failed to open default launcher's title.tmd\n");
		return false;
	}
	// Set back the title.tmd's title id from GNXX to HNXX
	fseek(launcherTmd, 0x190, SEEK_SET);
	char c;
	fread(&c, 1, 1, launcherTmd);
	//if byte is not what we expect, the install method was different
	if(c == 0x47)
	{
		fseek(launcherTmd, -1, SEEK_CUR);
		c = 0x48;
		fwrite(&c, 1, 1, launcherTmd);
	}
	else if(c != 0x47)
	{
		if (getFileSize(launcherTmd) > 520) {
			// Remove unlaunch if it already exists on the main launcher tmd.
			// If we don't do this and unlaunch is on the tmd, it will take over and prevent loading HNAA

			// This is also a good idea to make sure the tmd is 520b.
			// You will have a much higher brick risk if something goes wrong with a tmd over 520b.
			// See: http://docs.randommeaninglesscharacters.com/unlaunch.html
			messageBox(" Unlaunch was installed with the legacy method\nTrimming tmd\n");
			if (ftruncate(fileno(launcherTmd), 520) != 0) {
				messageBox("\x1B[31mError:\x1B[33m Failed to remove unlaunch\n");
	   			fclose(launcherTmd);
	   			return false;
   			}
   			fclose(launcherTmd);
	   		return true;
		}
		messageBox("\x1B[31mError:\x1B[33m Unlaunch was installed with an\nunknown method\naborting\n");
		fclose(launcherTmd);
		return false;
	}
	fclose(launcherTmd);
	return true;
}

static bool patchMainTmd(const char* path)
{
	FILE* launcherTmd = fopen(path, "r+b");
	if(!launcherTmd)
	{
		messageBox("\x1B[31mError:\x1B[33m Failed to open default launcher's title.tmd\n");
		return false;
	}
	// Patches the title.tmd's title id from HNXX to GNXX
	fseek(launcherTmd, 0x190, SEEK_SET);
	char c;
	fread(&c, 1, 1, launcherTmd);
	//if byte is not already set, it's clean
	if(c == 0x48)
	{
		fseek(launcherTmd, -1, SEEK_CUR);
		c = 0x47;
		fwrite(&c, 1, 1, launcherTmd);
	}
	else if(c != 0x47)
	{
		messageBox("\x1B[31mError:\x1B[33m Default launcher's title.tmd was tamprered with, aborting\n");
		fclose(launcherTmd);
		return false;
	}
	if (getFileSize(launcherTmd) > 520) {
		// Remove unlaunch if it already exists on the main launcher tmd.
		// If we don't do this and unlaunch is on the tmd, it will take over and prevent loading HNAA 
		messageBox("Unlaunch is already installed \nwith the legacy method\nTrying to remove...\n");
		if (ftruncate(fileno(launcherTmd), 520) != 0) {
			messageBox("\x1B[31mError:\x1B[33m Failed to remove old unlaunch\n");
   			fclose(launcherTmd);
   			return false;
		}
		fclose(launcherTmd);
   		return true;
	}
	fclose(launcherTmd);
	return true;
}

static bool restoreProtoTmd(const char* path)
{
	if (!fileExists(hnaaBackupTmdPath))
	{
		messageBox("\x1B[31mError:\x1B[33m No original tmd found!\nCan't uninstall unlaunch.\n");
		return false;
	}
	remove(path);
	rename(hnaaBackupTmdPath, path);
	toggleFileReadOnly(path, false);
	return true;
}

bool uninstallUnlaunch(bool retailConsole, const char* retailLauncherTmdPath)
{
	// TODO: handle retailLauncherTmdPresentAndToBePatched = false on retail consoles
	if (retailConsole) {
		if (!toggleFileReadOnly(retailLauncherTmdPath, false) || !restoreMainTmd(retailLauncherTmdPath))
		{
			return false;
		}
	} else {
		if (!toggleFileReadOnly("nand:/title/00030017/484e4141/content/title.tmd", false) || !restoreProtoTmd("nand:/title/00030017/484e4141/content/title.tmd"))
		{
			return false;
		}
	}
	return true;
}

static bool writeUnlaunchTmd(const char* path)
{
	FILE* targetTmd = fopen(path, "wb");
	if (!targetTmd)
	{
		messageBox("\x1B[31mError:\x1B[33m Failed to open target unlaunch tmd\n");
		return false;
	}

	Sha1Digest expectedDigest, actualDigest;
	swiSHA1Calc(expectedDigest.data(), unlaunchInstallerBuffer, unlaunchInstallerSize + 520);
	
	if(!writeToFile(targetTmd, unlaunchInstallerBuffer, unlaunchInstallerSize + 520))
	{
		fclose(targetTmd);
		remove(path);
		messageBox("\x1B[31mError:\x1B[33m Failed write unlaunch to tmd\n");
		return false;
	}
	fclose(targetTmd);
	calculateFileSha1Path(path, actualDigest.data());
	
	if(expectedDigest != actualDigest)
	{
		remove(path);
		messageBox("\x1B[31mError:\x1B[33m Unlaunch tmd was not properly written\n");
		return false;
	}
	return true;
}

static bool installUnlaunchRetailConsole(const char* retailLauncherTmdPath)
{
	//Create HNAA launcher folder
	if (!safeCreateDir("nand:/title/00030017")
		|| !safeCreateDir("nand:/title/00030017/484e4141")
		|| !safeCreateDir("nand:/title/00030017/484e4141/content")) {
		return false;
	}

	// We have to remove write protect otherwise reinstalling will fail.
	if (fileExists(hnaaTmdPath) && !toggleFileReadOnly(hnaaTmdPath, false)) {
		messageBox("\x1B[31mError:\x1B[33m Can't remove launcher tmd write protect\n");
		return false;
	}
	if (!writeUnlaunchTmd(hnaaTmdPath))
	{
		rmdir("nand:/title/00030017/484e4141/content");
		rmdir("nand:/title/00030017/484e4141");
		return false;
	}

	//Mark the tmd as readonly
	if(!toggleFileReadOnly(hnaaTmdPath, true))
	{
		messageBox("\x1B[31mError:\x1B[33m Failed to mark unlaunch's title.tmd as read only\n");
		remove("nand:/title/00030017/484e4141/content/title.tmd");
		rmdir("nand:/title/00030017/484e4141/content");
		rmdir("nand:/title/00030017/484e4141");
		return false;
	}

	//Finally patch the default launcher tmd to be invalid
	//If there isn't a title.tmd matching the language region in the hwinfo
	// nothing else has to be done, could be a language patch, or a dev system, the user will know what they have done
	if (retailLauncherTmdPath)
	{
		// Set tmd as writable in case unlaunch was already installed through the old method
		if(!toggleFileReadOnly(retailLauncherTmdPath, false) || !patchMainTmd(retailLauncherTmdPath))
		{
			if(!toggleFileReadOnly(hnaaTmdPath, false))
			{
				messageBox("\x1B[31mError:\x1B[33m Failed to mark unlaunch's title.tmd as writable\nLeaving as is\n");
			}
			else
			{
				remove("nand:/title/00030017/484e4141/content/title.tmd");
				rmdir("nand:/title/00030017/484e4141/content");
				rmdir("nand:/title/00030017/484e4141");
			}
			return false;
		}
		if (!toggleFileReadOnly(retailLauncherTmdPath, true))
		{
#if 0
			// TODO: Rollback or live with it?
			messageBox("\x1B[31mError:\x1B[33m Failed to mark default launcher's title.tmd\nas read only, reverting the changes\n");
			restoreMainTmd(retailLauncherTmdPath)
#endif
		}
	}
	return true;
}

static bool installUnlaunchProtoConsole(void)
{
	if(choiceBox("Your DSi has a non-standard\nregion.\n"
				"\x1B[31mInstalling unlaunch may be\n"
				"unsafe.\x1B[33m"
				"\nCancelling is recommended!"
				"\n\nContinue anyways?") == NO)
		return false;

	// Prototypes DSis are always HNAA. We can't use code that will nuke their launcher.

	// Also some justification for adding proto support: they're really common.
	// "Real" protos (X3, X4, etc) are hard to find but there are tons of release
	// version DSis that are running prototype firmware.
	// Likely factory rejects that never had production firmware flashed.

	// We have to remove write protect otherwise reinstalling will fail.
	if (fileExists(hnaaTmdPath) && !toggleFileReadOnly(hnaaTmdPath, false)) {
		messageBox("\x1B[31mError:\x1B[33m Can't remove launcher tmd write protect\n");
		return false;
	}
	bool hnaaBackupExists = fileExists(hnaaBackupTmdPath);
	// Back up the TMD since we'll be writing to it directly.
	if (!hnaaBackupExists)
	{
		rename(hnaaTmdPath, hnaaBackupTmdPath);
		// Mark backup tmd as readonly, just to be sure
		toggleFileReadOnly("nand:/title/00030017/484e4141/content/title.tmd.bak", true);
	}
	
	if(!writeUnlaunchTmd(hnaaTmdPath))
	{
		copyFile("nand:/title/00030017/484e4141/content/title.tmd.bak", hnaaTmdPath);
		return false;
	}
	
	// Mark the tmd as readonly
	if (!toggleFileReadOnly(hnaaTmdPath, true))
	{
		// There is nothing that can be done at this point.
		messageBox("\x1B[31mError:\x1B[33m Failed to mark tmd as read only\n");
	}
	return true;
}

bool readUnlaunchInstaller(const char* path)
{
	FILE* unlaunchInstaller = fopen(path, "rb");
	if (!unlaunchInstaller)
	{
		messageBox("\x1B[31mError:\x1B[33m Failed to open unlaunch installer\n(sd:/unlaunch.dsi)\n");
		return false;
	}
	
	unlaunchInstallerSize = getFileSize(unlaunchInstaller);
	if(isValidUnlaunchInstallerSize(unlaunchInstallerSize))
	{
		messageBox("\x1B[31mError:\x1B[33m Unlaunch installer wrong file size\n");
		return false;
	}

	int toRead = unlaunchInstallerSize;
	char* buff = unlaunchInstallerBuffer;
	// Pad the installer with 520 bytes, those being the size of a valid tmd
	buff += 520;

	size_t n = 0;
	while (toRead != 0 && (n = fread(buff, sizeof(char), toRead, unlaunchInstaller)) > 0)
	{
		toRead -= n;
		buff += n;
	}
	if (toRead != 0 || ferror(unlaunchInstaller))
	{
		fclose(unlaunchInstaller);
		messageBox("\x1B[31mError:\x1B[33m Failed read unlaunch installer\n");
		return false;
	}

	fclose(unlaunchInstaller);	
	return true;
}

bool verifyUnlaunchInstaller(void)
{
	Sha1Digest digest;
	swiSHA1Calc(digest.data(), unlaunchInstallerBuffer + 520,  unlaunchInstallerSize);
	auto it = std::ranges::find(knownUnlaunchHashes, digest);
	if(it == knownUnlaunchHashes.end())
	{
		messageBox("\x1B[31mError:\x1B[33m Provided unlaunch installer has an unknown hash\n");
		return false;
	}
	auto idx = std::distance(knownUnlaunchHashes.begin(), it);
	installerVersion = static_cast<UNLAUNCH_VERSION>(idx);
	return true;
}

bool patchUnlaunchInstaller(void)
{
	//TODO: Apply patches
	return true;
}

bool loadUnlaunchInstaller(const char* path)
{
	return readUnlaunchInstaller(path) && verifyUnlaunchInstaller();
}

bool installUnlaunch(bool retailConsole, const char* retailLauncherTmdPath)
{
	if (installerVersion == INVALID || !patchUnlaunchInstaller())
		return false;

	// Treat protos differently
	if (!retailConsole)
	{
		return installUnlaunchProtoConsole();
	}
	// Do things normally for production units
	return installUnlaunchRetailConsole(retailLauncherTmdPath);
}