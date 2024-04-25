#ifndef STORAGE_H
#define STORAGE_H

#include <nds/ndstypes.h>
#include <stdio.h>

//Files
bool fileExists(char const* path);
int copyFile(char const* src, char const* dst);
int copyFilePart(char const* src, u32 offset, u32 size, char const* dst);
unsigned long long getFileSize(FILE* f);
unsigned long long getFileSizePath(char const* path);
bool toggleFileReadOnly(const char* path, bool readOnly);
bool writeToFile(FILE* fd, const char* buffer, size_t size);

//Directories
bool safeCreateDir(const char* path);

#endif