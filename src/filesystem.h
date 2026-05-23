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
	uint64_t isFile    : 1;
	uint64_t isArchive : 1;
	uint64_t isExec    : 1;
	uint64_t size     : 61;
	Date createTime;
	Date writeTime;
	wchar_t* name;
} File;

typedef struct FileArray
{
	File* files;
	size_t count;
	size_t cap;
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
	SORT_TYPE_INV,
	SORT_LETTER,
	SORT_LETTER_INV,
};

void sortFileArray(FileArray* fileArray, int option);

wchar_t* moveDirUp(wchar_t* dir, bool* error);
wchar_t* moveDirDown(wchar_t* dir, const wchar_t* subdir, bool* error);



typedef struct Drive
{
	wchar_t* name;
	wchar_t* path; // attempts to use drive letter path, else uses long path
	size_t capacity;
	size_t free;
} Drive;

typedef struct DriveArray
{
	Drive* drives;
	size_t count;
	size_t cap;
} DriveArray;

DriveArray getDrives(void);

void freeDriveArray(DriveArray* array);

FileArray getDrivesAsFileArray(void);

void sortDriveArray(DriveArray* driveArray, int option);


wchar_t* getCurrentDir(void);


#include <Windows.h>

typedef struct
{
	HANDLE hDir;
	OVERLAPPED overlapped;
	void* buf;
	DWORD bufSize;
} WatchDirInfo;

bool watchDirStart(const wchar_t* path, WatchDirInfo* info);

FILE_NOTIFY_INFORMATION* watchDirIterate(WatchDirInfo* info);

void watchDirStop(WatchDirInfo* info);

bool checkDirUpdates(WatchDirInfo* info, FileArray* fileArray, const wchar_t* dir, int sortMethod);