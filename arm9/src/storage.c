#include "storage.h"
#include "main.h"
#include "message.h"
#include <sys/stat.h>
#include <errno.h>
#include <nds/sha1.h>
#include <dirent.h>
#include <sys/statvfs.h>

//progress bar
static int lastBars = 0;

static void printProgressBar(float percent)
{
	if (percent < 0.f) percent = 0.f;
	if (percent > 1.f) percent = 1.f;

	int bars = (int)(30.f * percent);

	//skip redundant prints
	if (bars != lastBars)
	{
		consoleSelect(&topScreen);

		printf("\x1B[42m");	//green

		//Print frame
		if (lastBars <= 0)
		{
			printf("\x1b[23;0H[");
			printf("\x1b[23;31H]");
		}

		//Print bars
		if (bars > 0)
		{
			for (int i = 0; i < bars; i++)
				printf("\x1b[23;%dH|", 1 + i);
		}

		lastBars = bars;

		printf("\x1B[47m");	//white
	}
}

static void clearProgressBar()
{
	lastBars = 0;
	consoleSelect(&topScreen);
	printf("\x1b[23;0H                                ");
}

//files
bool fileExists(char const* path)
{
	return access(path, F_OK) == 0;
}

int copyFile(char const* src, char const* dst)
{
	if (!src) return 1;

	unsigned long long size = getFileSizePath(src);
	return copyFilePart(src, 0, size, dst);
}

int copyFilePart(char const* src, u32 offset, u32 size, char const* dst)
{
	if (!src) return 1;
	if (!dst) return 2;

	FILE* fin = fopen(src, "rb");

	if (!fin)
	{
		return 3;
	}
	else
	{
		if (fileExists(dst))
			remove(dst);

		FILE* fout = fopen(dst, "wb");

		if (!fout)
		{
			fclose(fin);
			return 4;
		}
		else
		{
			fseek(fin, offset, SEEK_SET);

			consoleSelect(&topScreen);

			int bytesRead;
			unsigned long long totalBytesRead = 0;

			#define BUFF_SIZE 128 //Arbitrary. A value too large freezes the ds.
			char* buffer = (char*)malloc(BUFF_SIZE);

			while (!programEnd)
			{
				unsigned int toRead = BUFF_SIZE;
				if (size - totalBytesRead < BUFF_SIZE)
					toRead = size - totalBytesRead;

				bytesRead = fread(buffer, 1, toRead, fin);
				fwrite(buffer, bytesRead, 1, fout);

				totalBytesRead += bytesRead;
				printProgressBar( ((float)totalBytesRead / (float)size) );

				if (bytesRead != BUFF_SIZE)
					break;
			}

			clearProgressBar();
			consoleSelect(&bottomScreen);

			free(buffer);
		}

		fclose(fout);
	}

	fclose(fin);
	return 0;
}

unsigned long long getFileSize(FILE* f)
{
	if (!f) return 0;

	fseek(f, 0, SEEK_END);
	unsigned long long size = ftell(f);
	fseek(f, 0, SEEK_SET);

	return size;
}

unsigned long long getFileSizePath(char const* path)
{
	if (!path) return 0;

	FILE* f = fopen(path, "rb");
	unsigned long long size = getFileSize(f);
	if(f)
		fclose(f);

	return size;
}

bool toggleFileReadOnly(const char* path, bool readOnly)
{
	int fatAttributes = FAT_getAttr(path);
	if (readOnly)
		fatAttributes |= ATTR_READONLY;
	else
		fatAttributes &= ~ATTR_READONLY;
	return FAT_setAttr(path, fatAttributes) == 0;
}

bool writeToFile(FILE* fd, const char* buffer, size_t size)
{
	int toWrite = size;
	size_t written;
	//write the first 520 bytes as 0, as that's the size of a tmd, but it can be whatever content
	while (toWrite > 0 && (written = fwrite(buffer, sizeof(char), toWrite, fd)) > 0)
	{
		toWrite -= written;
		buffer += written;
	}
	return toWrite == 0;
}

bool calculateFileSha1Offset(FILE* f, void* digest, size_t offset)
{
	fseek(f, offset, SEEK_SET);
	
	swiSHA1context_t ctx;
	ctx.sha_block = 0; //this is weird but it has to be done
	swiSHA1Init(&ctx);
	
	char buffer[512];	
	size_t n = 0;
	while ((n = fread(buffer, sizeof(char), sizeof(buffer), f)) > 0)
	{
		swiSHA1Update(&ctx, buffer, n);
	}
	if (ferror(f) || !feof(f))
	{
		return false;
	}
	swiSHA1Final(digest, &ctx);
	return true;
}

bool calculateFileSha1PathOffset(const char* path, void* digest, size_t offset)
{
	FILE* targetFile = fopen(path, "rb");
	if (!targetFile)
	{
		return false;
	}	
	bool res = calculateFileSha1Offset(targetFile, digest, offset);
	fclose(targetFile);
	return res;
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

bool removeIfExists(const char* path)
{
	if(remove(path) == 0 || errno == ENOENT) {
		return true;
	}
	return rmdir(path) == 0 || errno == ENOENT;
}

u32 getClusterSizeForPartition(const char* path)
{
	struct statvfs s;
	if(statvfs(path, &s) != 0)
		return 0;
	return s.f_bsize;
}
