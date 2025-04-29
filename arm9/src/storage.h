#ifndef STORAGE_H
#define STORAGE_H

#include <nds/ndstypes.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

//Files
bool fileExists(char const* path);
int copyFile(char const* src, char const* dst);
int copyFilePart(char const* src, u32 offset, u32 size, char const* dst);
unsigned long long getFileSize(FILE* f);
unsigned long long getFileSizePath(char const* path);
bool toggleFileReadOnly(const char* path, bool readOnly);
bool writeToFile(FILE* fd, const char* buffer, size_t size);
bool calculateFileSha1(FILE* f, void* digest);
bool calculateFileSha1Path(const char* path, void* digest);

//Directories
bool safeCreateDir(const char* path);

//Files and directories
bool removeIfExists(const char* path);

u32 getClusterSizeForPartition(const char* path);

#ifdef __cplusplus
}
#endif

#endif