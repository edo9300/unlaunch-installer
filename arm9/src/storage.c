#include "storage.h"
#include "main.h"
#include "message.h"
#include <errno.h>
#include <nds/sha1.h>
#include <dirent.h>

#define TITLE_LIMIT 39

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

		iprintf("\x1B[42m");	//green

		//Print frame
		if (lastBars <= 0)
		{
			iprintf("\x1b[23;0H[");
			iprintf("\x1b[23;31H]");
		}

		//Print bars
		if (bars > 0)
		{
			for (int i = 0; i < bars; i++)
				iprintf("\x1b[23;%dH|", 1 + i);
		}

		lastBars = bars;

		iprintf("\x1B[47m");	//white
	}
}

static void clearProgressBar()
{
	lastBars = 0;
	consoleSelect(&topScreen);
	iprintf("\x1b[23;0H                                ");
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
		fclose(fin);
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
			fclose(fout);
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

bool calculateFileSha1(FILE* f, void* digest)
{
	fseek(f, 0, SEEK_SET);
	
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

bool calculateFileSha1Path(const char* path, void* digest)
{
	FILE* targetFile = fopen(path, "rb");
	if (!targetFile)
	{
		return false;
	}	
	bool res = calculateFileSha1(targetFile, digest);
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
	return remove(path) == 0 || errno == ENOENT;
}

// Filesystem type
typedef enum {FS_UNKNOWN, FS_FAT12, FS_FAT16, FS_FAT32} FS_TYPE;
//trimmed down PARTITION struct from libfat internals
typedef struct {
	const void* disc;
	void*                cache;
	// Info about the partition
	FS_TYPE               filesysType;
	uint64_t              totalSize;
	sec_t                 rootDirStart;
	uint32_t              rootDirCluster;
	uint32_t              numberOfSectors;
	sec_t                 dataStart;
	uint32_t              bytesPerSector;
	uint32_t              sectorsPerCluster;
	uint32_t              bytesPerCluster;
	uint32_t              fsInfoSector;
} PARTITION;

extern PARTITION* _FAT_partition_getPartitionFromPath(const char* path);

u32 getClusterSizeForPartition(const char* path)
{
	PARTITION* p = _FAT_partition_getPartitionFromPath(path);
	if(!p)
		return 0;
	return p->bytesPerCluster;
}
