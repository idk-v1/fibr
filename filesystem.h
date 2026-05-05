#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct Date
{
	uint32_t minute : 6;
	uint32_t hour   : 5;
	uint32_t day    : 5;
	uint32_t month  : 4;
	uint32_t year  : 12;
} Date;

typedef struct File
{
	uint64_t isFile   : 1;
	uint64_t size    : 63;
	Date createTime;
	Date writeTime;
	wchar_t* name;
} File;

typedef struct FileArray
{
	File* files;
	size_t count;
} FileArray;


FileArray getFilesInDir(const wchar_t* path, bool subdirCount);

size_t getFileCountInDir(const wchar_t* path);

wchar_t* strSubdir(const wchar_t* dir, const wchar_t* sub);

void freeFileArray(FileArray* fileArray);


enum
{
	SORT_NAME,
	SORT_NAME_INV,
	SORT_SIZE,
	SORT_SIZE_INV,
	SORT_CREATE,
	SORT_CREATE_INV,
	SORT_WRITE,
	SORT_WRITE_INV,
	SORT_TYPE,
	SORT_TYPE_INV
};

void sortFileArray(FileArray fileArray, int option);

wchar_t* moveDirUp(wchar_t* dir);
wchar_t* moveDirDown(wchar_t* dir, const wchar_t* subdir);