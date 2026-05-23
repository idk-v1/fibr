#include "filesystem.h"

#include <Windows.h>


static bool shouldHideDrive(const wchar_t* name)
{
	const wchar_t* names[] =
	{
		L"WINRE_DRV",
		L"SYSTEM_DRV"
	};

	int len = (int)wcslen(name);

	for (size_t i = 0; i < sizeof(names) / sizeof(wchar_t*); ++i)
	{
		if (lstrcmpiW(name, names[i]) == 0)
			return true;
	}

	return false;
}

DriveArray getDrives(void)
{
	wchar_t buf[MAX_PATH] = { 0 };
	DWORD bufsize = MAX_PATH;

	HANDLE hVol = FindFirstVolumeW(buf, bufsize);
	if (hVol == INVALID_HANDLE_VALUE)
		return (DriveArray){0};
	
	DWORD available = GetLogicalDrives();
	size_t bitcount = 0;
	for (size_t i = 0; i < 32; ++i)
	{
		bitcount += (available & 1);
		available >>= 1;
	}

	DriveArray array = { 0 };
	// for more that 26, unknown number, just double
	array.cap = bitcount;
	array.drives = malloc(sizeof(Drive) * array.cap);
	if (!array.drives)
	{
		FindVolumeClose(hVol);
		return (DriveArray){0};
	}

	while (true)
	{
		wchar_t devName[MAX_PATH] = {0};

		// Hidden partitions, hopefully consistantly named
		// Need to ignore bc can't even access them
		GetVolumeInformationW(buf, devName, MAX_PATH, NULL, NULL, NULL, NULL, 0);
		if (!shouldHideDrive(devName))
		{
			if (array.count + 1 > array.cap)
			{
				array.cap *= 2;
				Drive* temp = realloc(array.drives, sizeof(Drive) * array.cap);
				if (temp)
					array.drives = temp;
				else
				{
					// return what we have
					FindVolumeClose(hVol);
					return array;
				}
			}

			Drive drive = { 0 };

			ULARGE_INTEGER totalSpace = { 0 }, freeSpace = { 0 };
			GetDiskFreeSpaceExW(buf, NULL, &totalSpace, &freeSpace);
			drive.capacity = totalSpace.QuadPart;
			drive.free = freeSpace.QuadPart;

			size_t len = wcslen(devName);
			drive.name = malloc(sizeof(wchar_t) * (len + 1));
			if (!drive.name)
			{
				FindVolumeClose(hVol);
				return array;
			}
			memcpy(drive.name, devName, sizeof(wchar_t) * (len + 1));

			DWORD volNameSize = 0;
			GetVolumePathNamesForVolumeNameW(buf, devName, MAX_PATH, &volNameSize);
			const wchar_t* path = NULL;
			if (volNameSize > 1)
			{
				len = (size_t)volNameSize - 2; // size includes NULL and trailing slash
				path = devName;
			}
			else // no drive letter
			{
				len = wcslen(buf) - 4;
				path = buf + 4;
			}
				
			drive.path = malloc(sizeof(wchar_t) * (len + 1));
			if (!drive.path)
			{
				// return what we have
				free(drive.name);
				FindVolumeClose(hVol);
				return array;
			}
			memcpy(drive.path, path, sizeof(wchar_t) * len);
			
			drive.path[len - 1] = 0;
			array.drives[array.count] = drive;
			++array.count;
		}

		if (!FindNextVolumeW(hVol, buf, bufsize))
			break;
	}

	FindVolumeClose(hVol);
	return array;
}

void freeDriveArray(DriveArray* array)
{
	if (array)
	{
		if (array->drives)
		{
			for (size_t i = 0; i < array->count; ++i)
			{
				free(array->drives[i].name);
				free(array->drives[i].path);
			}
			free(array->drives);
			array->drives = NULL;
		}
		array->count = 0;
		array->cap = 0;
	}
}

FileArray getDrivesAsFileArray(void)
{
	DriveArray drives = getDrives();

	FileArray array = { 0 };
	array.count = drives.count;
	array.cap = array.count;
	array.files = malloc(sizeof(File) * array.count);
	if (!array.files)
	{
		freeDriveArray(&drives);
		return (FileArray){0};
	}

	for (size_t i = 0; i < array.count; ++i)
	{
		File file = { 0 };
		file.name = drives.drives[i].path; // just steal the child and kill the parent
		free(drives.drives[i].name);
		array.files[i] = file;
	}
	free(drives.drives);

	return array;
}


static Date dateFromFiletime(FILETIME ft)
{
	Date date = { 0 };

	FILETIME localFt = ft;
	if (FileTimeToLocalFileTime(&ft, &localFt))
	{
		SYSTEMTIME sys;
		if (FileTimeToSystemTime(&localFt, &sys))
		{
			date.day = sys.wDay;
			date.month = sys.wMonth;
			date.year = sys.wYear;
			date.minute = sys.wMinute;
			date.hour = sys.wHour;
		}
	}

	return date;
}

static bool isArchive(const wchar_t* name)
{
	const wchar_t* exts[] =
	{
		L"zip",
		L"7z",
		L"tar",
		L"gz",
		L"xz",
		L"asc"
	};

	int len = (int)wcslen(name);
	int extPos = -1;
	for (int i = len - 1; i >= 0; --i)
	{
		if (name[i] == L'.')
		{
			extPos = i + 1;
			break;
		}
	}
	if (extPos == -1)
		return false;

	for (size_t i = 0; i < sizeof(exts) / sizeof(wchar_t*); ++i)
	{
		if (lstrcmpiW(name + extPos, exts[i]) == 0)
			return true;
	}

	return false;
}


static bool isExecutable(const wchar_t* name)
{
	const wchar_t* exts[] =
	{
		L"exe",
		L"msi",
		L"scr",
		L"msc",
		L"ps1",
		L"cmd",
		L"bat",
		L"com",
	};

	int len = (int)wcslen(name);
	int extPos = -1;
	for (int i = len - 1; i >= 0; --i)
	{
		if (name[i] == L'.')
		{
			extPos = i + 1;
			break;
		}
	}
	if (extPos == -1)
		return false;

	for (size_t i = 0; i < sizeof(exts) / sizeof(wchar_t*); ++i)
	{
		if (lstrcmpiW(name + extPos, exts[i]) == 0)
			return true;
	}

	return false;
}

FileArray getFilesInDir(const wchar_t* path, bool subdirCount)
{
	if (!path)
		return (FileArray){0};

	size_t pathLen = wcslen(path);
	if (pathLen == 0)
		return (FileArray){0};

	wchar_t* wildPath = malloc(sizeof(wchar_t) * (pathLen + 3));
	if (!wildPath)
		return (FileArray){0};

	for (size_t i = 0; i < pathLen; ++i)
	{
		if (path[i] == L'/')
			wildPath[i] = L'\\';
		else
			wildPath[i] = path[i];
	}
	
	// FileFirstFile needs a path to end in a wildcard
	// "thing\*"
	if (path[pathLen - 1] == L'\\')
	{
		wildPath[pathLen] = L'*';
		wildPath[pathLen + 1] = 0;
	}
	else
	{
		wildPath[pathLen] = L'\\';
		wildPath[pathLen + 1] = L'*';
		wildPath[pathLen + 2] = 0;
	}

	
	WIN32_FIND_DATAW findData = { 0 };
	HANDLE hFind = FindFirstFileExW(wildPath, FindExInfoBasic, &findData, 
		FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		free(wildPath);
		return (FileArray){0};
	}
	free(wildPath);
	// the fake directories "." and ".." always appear first
	FindNextFileW(hFind, &findData);


	FileArray array = { 0 };
	array.cap = 16;

	array.files = malloc(sizeof(File) * array.cap);
	if (!array.files)
	{
		FindClose(hFind);
		return (FileArray) { 0 };
	}


	while (FindNextFileW(hFind, &findData))
	{
		if (array.count + 1 > array.cap)
		{
			array.cap *= 2;
			File* temp = realloc(array.files, sizeof(File) * array.cap);
			if (temp)
				array.files = temp;
			else break; // be happy with what we've got
		}

		File file = { 0 };
		file.isFile = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;

		size_t nameLen = wcslen(findData.cFileName);
		size_t filenameStart = 0;
		for (size_t i = nameLen - 1; i > 0; --i)
			if (findData.cFileName[i] == L'\\')
			{
				filenameStart = i + 1;
				break;
			}
		file.name = malloc(sizeof(wchar_t) * (nameLen - filenameStart + 1));
		if (!file.name)
			break; // be happy with what we've got
		memcpy(file.name, findData.cFileName, sizeof(wchar_t) * (nameLen - filenameStart + 1));

		if (file.isFile)
		{
			LARGE_INTEGER fileSize = { .HighPart = findData.nFileSizeHigh, .LowPart = findData.nFileSizeLow };
			file.size = fileSize.QuadPart;

			file.isArchive = isArchive(file.name);
			file.isExec = isExecutable(file.name);
		}
		else if (subdirCount)
		{
			wchar_t* subdir = strSubdir(path, file.name);
			file.size = getFileCountInDir(subdir);
			free(subdir);
			file.isArchive = 0;
		}
		else
		{
			file.size = 0;
			file.isArchive = 0;
		}

		file.createTime = dateFromFiletime(findData.ftCreationTime);
		file.writeTime = dateFromFiletime(findData.ftLastWriteTime);

		array.files[array.count] = file;
		++array.count;
	}
	FindClose(hFind);

	return array;
}

size_t getFileCountInDir(const wchar_t* path)
{
	if (!path)
		return 0;

	size_t pathLen = wcslen(path);
	if (pathLen == 0)
		return 0;

	wchar_t* wildPath = malloc(sizeof(wchar_t) * (pathLen + 3));
	if (!wildPath)
		return 0;

	for (size_t i = 0; i < pathLen; ++i)
	{
		if (path[i] == L'/')
			wildPath[i] = L'\\';
		else
			wildPath[i] = path[i];
	}

	// FileFirstFile needs a path to end in a wildcard
	// "thing\*"
	if (path[pathLen - 1] == L'\\')
	{
		wildPath[pathLen] = L'*';
		wildPath[pathLen + 1] = 0;
	}
	else
	{
		wildPath[pathLen] = L'\\';
		wildPath[pathLen + 1] = L'*';
		wildPath[pathLen + 2] = 0;
	}


	WIN32_FIND_DATAW findData = { 0 };
	HANDLE hFind = FindFirstFileExW(wildPath, FindExInfoBasic, &findData,
		FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
	free(wildPath);
	if (hFind == INVALID_HANDLE_VALUE)
		return 0;

	// the fake directories "." and ".." always appear first, skip
	FindNextFileW(hFind, &findData);


	size_t count = 0;

	while (FindNextFileW(hFind, &findData))
		++count;

	FindClose(hFind);

	return count;
}

wchar_t* strSubdir(const wchar_t* dir, const wchar_t* sub)
{
	if (!dir || !sub)
		return NULL;

	size_t dirLen = wcslen(dir);
	size_t subLen = wcslen(sub);

	wchar_t* newdir = malloc(sizeof(wchar_t) * (dirLen + subLen + 2));
	if (!newdir)
		return NULL;

	memcpy(newdir, dir, sizeof(wchar_t) * dirLen);
	newdir[dirLen] = L'\\';
	memcpy(newdir + 1 + dirLen, sub, sizeof(wchar_t) * subLen);
	newdir[dirLen + subLen + 1] = 0;
	return newdir;
}

void freeFileArray(FileArray* fileArray)
{
	if (fileArray)
	{
		if (fileArray->files)
		{
			for (size_t i = 0; i < fileArray->count; ++i)
				free(fileArray->files[i].name);
			free(fileArray->files);
			fileArray->files = NULL;
		}
		fileArray->count = 0;
		fileArray->cap = 0;
	}
}


static int cmpFilesName(const wchar_t* a, const wchar_t* b)
{
	return lstrcmpiW(a, b);
}

static int cmpFilesSize(size_t a, size_t b)
{
	return (a > b ? 1 : -1);
}

static int cmpFilesDate(Date a, Date b)
{
	if (a.year != b.year)
		return (a.year > b.year ? 1 : -1);
	if (a.month != b.month)
		return (a.month > b.month ? 1 : -1);
	if (a.day != b.day)
		return (a.day > b.day ? 1 : -1);
	if (a.hour != b.hour)
		return (a.hour > b.hour ? 1 : -1);
	if (a.minute != b.minute)
		return (a.minute > b.minute ? 1 : -1);
	return 0;
}

static int cmpFilesType(const wchar_t* a, const wchar_t* b)
{
	size_t aLen = wcslen(a);
	size_t bLen = wcslen(b);

	size_t aExt = aLen - 1;
	size_t bExt = bLen - 1;

	for (size_t i = aLen - 1; i > 0; --i)
		if (a[i] == L'.')
		{
			aExt = i + 1;
			break;
		}

	for (size_t i = bLen - 1; i > 0; --i)
		if (b[i] == L'.')
		{
			bExt = i + 1;
			break;
		}

	return cmpFilesName(a + aExt, b + bExt);
}

static int cmpFilesNameTop(File const* a, File const* b)
{
	if (!a->isFile && b->isFile)
		return 1;
	if (a->isFile && !b->isFile)
		return -1;

	return -cmpFilesName(a->name, b->name);
}

static int cmpFilesNameInvTop(File const* a, File const* b)
{
	if (!a->isFile && b->isFile)
		return 1;
	if (a->isFile && !b->isFile)
		return -1;

	return cmpFilesName(a->name, b->name);
}

static int cmpFilesSizeTop(File const* a, File const* b)
{
	if (!a->isFile && b->isFile)
		return 1;
	if (a->isFile && !b->isFile)
		return -1;

	int ret = -cmpFilesSize(a->size, b->size);
	if (ret) return ret;

	return -cmpFilesName(a->name, b->name);
}

static int cmpFilesSizeInvTop(File const* a, File const* b)
{
	if (!a->isFile && b->isFile)
		return 1;
	if (a->isFile && !b->isFile)
		return -1;

	int ret = cmpFilesSize(a->size, b->size);
	if (ret) return ret;

	return -cmpFilesName(a->name, b->name);
}

static int cmpFilesCreateTop(File const* a, File const* b)
{
	if (!a->isFile && b->isFile)
		return 1;
	if (a->isFile && !b->isFile)
		return -1;

	int ret = -cmpFilesDate(a->createTime, b->createTime);
	if (ret) return ret;

	return -cmpFilesName(a->name, b->name);
}

static int cmpFilesCreateInvTop(File const* a, File const* b)
{
	if (!a->isFile && b->isFile)
		return 1;
	if (a->isFile && !b->isFile)
		return -1;

	int ret = cmpFilesDate(a->createTime, b->createTime);
	if (ret) return ret;

	return -cmpFilesName(a->name, b->name);
}

static int cmpFilesWriteTop(File const* a, File const* b)
{
	if (!a->isFile && b->isFile)
		return 1;
	if (a->isFile && !b->isFile)
		return -1;

	int ret = -cmpFilesDate(a->writeTime, b->writeTime);
	if (ret) return ret;

	return -cmpFilesName(a->name, b->name);
}

static int cmpFilesWriteInvTop(File const* a, File const* b)
{
	if (!a->isFile && b->isFile)
		return 1;
	if (a->isFile && !b->isFile)
		return -1;

	int ret = cmpFilesDate(a->writeTime, b->writeTime);
	if (ret) return ret;

	return -cmpFilesName(a->name, b->name);
}

static int cmpFilesTypeTop(File const* a, File const* b)
{
	if (!a->isFile && b->isFile)
		return 1;
	if (a->isFile && !b->isFile)
		return -1;

	if (a->isFile)
	{
		int ret = -cmpFilesType(a->name, b->name);
		if (ret) return ret;
	}

	return -cmpFilesName(a->name, b->name);
}

static int cmpFilesTypeInvTop(File const* a, File const* b)
{
	if (!a->isFile && b->isFile)
		return 1;
	if (a->isFile && !b->isFile)
		return -1;

	if (a->isFile)
	{
		int ret = cmpFilesType(a->name, b->name);
		if (ret) return ret;
	}

	return -cmpFilesName(a->name, b->name);
}


void sortFileArray(FileArray* fileArray, int option)
{
	if (!fileArray->files) return;

	int (*cmpFnc)(void const*, void const*) = NULL;
	switch (option)
	{
	case SORT_NAME: cmpFnc = cmpFilesNameTop; break;
	case SORT_NAME_INV: cmpFnc = cmpFilesNameInvTop; break;
	case SORT_SIZE: cmpFnc = cmpFilesSizeTop; break;
	case SORT_SIZE_INV: cmpFnc = cmpFilesSizeInvTop; break;
	case SORT_CREATE: cmpFnc = cmpFilesCreateTop; break;
	case SORT_CREATE_INV: cmpFnc = cmpFilesCreateInvTop; break;
	case SORT_WRITE: cmpFnc = cmpFilesWriteTop; break;
	case SORT_WRITE_INV: cmpFnc = cmpFilesWriteInvTop; break;
	case SORT_TYPE: cmpFnc = cmpFilesTypeTop; break;
	case SORT_TYPE_INV: cmpFnc = cmpFilesTypeInvTop; break;
	}

	if (cmpFnc)
		qsort(fileArray->files, fileArray->count, sizeof(File), cmpFnc);
}


static int cmpDrivesNameTop(Drive const* a, Drive const* b)
{
	int ret = -cmpFilesName(a->name, b->name);
	if (ret) return ret;

	return -cmpFilesName(a->path, b->path);
}

static int cmpDrivesNameInvTop(Drive const* a, Drive const* b)
{
	int ret = cmpFilesName(a->name, b->name);
	if (ret) return ret;

	return -cmpFilesName(a->path, b->path);
}

static int cmpDrivesSizeTop(Drive const* a, Drive const* b)
{
	int ret = -cmpFilesSize(a->free, b->free);
	if (ret) return ret;

	ret = -cmpFilesName(a->name, b->name);
	if (ret) return ret;

	return -cmpFilesName(a->path, b->path);
}

static int cmpDrivesSizeInvTop(Drive const* a, Drive const* b)
{
	int ret = cmpFilesSize(a->free, b->free);
	if (ret) return ret;

	ret = -cmpFilesName(a->name, b->name);
	if (ret) return ret;

	return -cmpFilesName(a->path, b->path);
}

static int cmpDrivesPathTop(Drive const* a, Drive const* b)
{
	return -cmpFilesName(a->path, b->path);
}

static int cmpDrivesPathInvTop(Drive const* a, Drive const* b)
{
	return cmpFilesName(a->path, b->path);
}

void sortDriveArray(DriveArray* driveArray, int option)
{
	if (!driveArray->drives) return;

	int (*cmpFnc)(void const*, void const*) = NULL;
	switch (option)
	{
	case SORT_NAME: cmpFnc = cmpDrivesNameTop; break;
	case SORT_NAME_INV: cmpFnc = cmpDrivesNameInvTop; break;
	case SORT_SIZE: cmpFnc = cmpDrivesSizeTop; break;
	case SORT_SIZE_INV: cmpFnc = cmpDrivesSizeInvTop; break;
	case SORT_LETTER: cmpFnc = cmpDrivesPathTop; break;
	case SORT_LETTER_INV: cmpFnc = cmpDrivesPathInvTop; break;
	}

	if (cmpFnc)
		qsort(driveArray->drives, driveArray->count, sizeof(Drive), cmpFnc);
}


wchar_t* moveDirUp(wchar_t* dir, bool* error)
{
	*error = false;
	if (!dir)
		return NULL;
	size_t len = wcslen(dir);

	size_t pos = len - 1;

	for (size_t i = len - 1; i > 0; --i)
	{
		if (dir[i] == L'\\')
		{
			pos = i;
			break;
		}
		if (dir[i] == L'?') // no more
		{
			*error = true;
			return dir;
		}
	}

	if (pos == 0) // no more
	{
		*error = true;
		return dir;
	}
	dir[pos] = 0;
	return dir;
}

wchar_t* moveDirDown(wchar_t* dir, const wchar_t* subdir, bool* error)
{
	*error = false;
	if (!dir)
		return NULL;
	if (!subdir)
	{
		*error = true;
		return dir;
	}

	size_t dirLen = wcslen(dir);
	if (wcslen(dir) == 4)
		dir[dirLen - 1] = 0;
	
	wchar_t* newDir = strSubdir(dir, subdir);

	free(dir);
	return newDir;
}


wchar_t* getCurrentDir(void)
{
	size_t pathLen = GetCurrentDirectoryW(0, NULL); // returns size needed including null
	wchar_t* dir = malloc(sizeof(wchar_t) * (pathLen + 4));
	if (!dir)
		return NULL;

	GetCurrentDirectoryW((DWORD)pathLen, dir + 4);

	dir[0] = L'\\';
	dir[1] = L'\\';
	dir[2] = L'?';
	dir[3] = L'\\';

	return dir;
}



bool watchDirStart(const wchar_t* path, WatchDirInfo* info)
{
	if (path)
	{
		info->hDir = CreateFileW(path,
			FILE_LIST_DIRECTORY, FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);

		if (info->hDir != INVALID_HANDLE_VALUE)
		{
			info->bufSize = USHRT_MAX;
			info->buf = malloc(USHRT_MAX);

			if (info->buf)
			{
				info->overlapped.hEvent = CreateEventW(NULL, false, false, NULL);
				if (info->overlapped.hEvent)
				{
					if (ReadDirectoryChangesW(info->hDir, info->buf, info->bufSize, false,
						FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
						FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE |
						FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION,
						NULL, &info->overlapped, NULL))
					{
						return true;
					}

					CloseHandle(info->overlapped.hEvent);
				}

				free(info->buf);
			}

			CloseHandle(info->hDir);
		}
	}

	memset(info, 0, sizeof(WatchDirInfo));
	return false;
}

FILE_NOTIFY_INFORMATION* watchDirIterate(WatchDirInfo* info)
{
	static size_t dataPos = -1;

	if (dataPos != -1)
	{
		FILE_NOTIFY_INFORMATION* fni = (FILE_NOTIFY_INFORMATION*)((char*)(info->buf) + dataPos);

		if (fni->NextEntryOffset == 0)
		{
			ReadDirectoryChangesW(info->hDir, info->buf, info->bufSize, false,
				FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
				FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE |
				FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION,
				NULL, &info->overlapped, NULL);

			dataPos = -1;
			return NULL;
		}
		else
		{
			dataPos += fni->NextEntryOffset;
			fni = (FILE_NOTIFY_INFORMATION*)((char*)(info->buf) + dataPos);
			return fni;
		}
	}
	else
	{
		DWORD state = WaitForSingleObject(info->overlapped.hEvent, 0);
		if (state == WAIT_OBJECT_0)
		{
			DWORD retSize = 0;
			GetOverlappedResult(info->hDir, &info->overlapped, &retSize, false);

			dataPos = 0;
			return info->buf;
		}
		else
		{
			dataPos = -1;
			return NULL;
		}
	}
}

void watchDirStop(WatchDirInfo* info)
{
	if (info)
	{
		if (info->buf)
			free(info->buf);

		CloseHandle(info->hDir);

		CloseHandle(info->overlapped.hEvent);

		memset(info, 0, sizeof(WatchDirInfo));
	}
}


static bool isFileDir(const wchar_t* file)
{
	return GetFileAttributesW(file) & FILE_ATTRIBUTE_DIRECTORY;
}

static bool getFileInfo(File* file, const wchar_t* dir)
{
	wchar_t* fullName = strSubdir(dir, file->name);
	if (fullName)
	{
		file->isFile = !isFileDir(fullName);

		HANDLE handle = CreateFileW(fullName, GENERIC_READ,
			FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE,
			NULL, OPEN_EXISTING, (file->isFile ? FILE_ATTRIBUTE_NORMAL : FILE_FLAG_BACKUP_SEMANTICS), NULL);
		if (handle != INVALID_HANDLE_VALUE)
		{
			if (file->isFile)
			{
				file->isArchive = isArchive(file->name);
				file->isExec = isExecutable(file->name);

				LARGE_INTEGER size = { 0 };
				GetFileSizeEx(handle, &size);
				file->size = size.QuadPart;
			}
			else
			{
				file->size = getFileCountInDir(fullName);
			}

			FILETIME create = { 0 }, write = { 0 };
			GetFileTime(handle, &create, NULL, &write);

			file->createTime = dateFromFiletime(create);
			file->writeTime = dateFromFiletime(write);

			CloseHandle(handle);
		}
		else
		{
			free(fullName);
			return false;
		}

		free(fullName);
		return true;
	}
	else
	{
		return false;
	}
}

static size_t fileArrayFind(FileArray* fileArray, const wchar_t* name)
{
	if (name)
	{
		for (size_t i = 0; i < fileArray->count; ++i)
		{
			if (wcscmp(name, fileArray->files[i].name) == 0)
				return i;
		}
	}

	return -1;
}


bool checkDirUpdates(WatchDirInfo* info, FileArray* fileArray, const wchar_t* dir, int sortMethod)
{
	bool ret = false;
	bool needsResort = false;

	size_t replaceFileIndex = -1;

	FILE_NOTIFY_INFORMATION* fni = NULL;
	while (fni = watchDirIterate(info))
	{
		switch (fni->Action)
		{
		case FILE_ACTION_ADDED:
		{
			if (fileArray->count + 1 > fileArray->cap)
			{
				// Doubling would probably be a waste for the occasional added file
				File* temp = realloc(fileArray->files, sizeof(File) * (fileArray->cap + 5));
				if (temp)
				{
					fileArray->files = temp;
					fileArray->cap += 5;
				}
				else
					break;
			}

			File file = { 0 };
			file.name = malloc(fni->FileNameLength + sizeof(wchar_t));
			if (file.name)
			{
				memcpy(file.name, fni->FileName, fni->FileNameLength);
				file.name[fni->FileNameLength / sizeof(wchar_t)] = 0;

				if (getFileInfo(&file, dir))
				{
					fileArray->files[fileArray->count] = file;
					++fileArray->count;

					needsResort = true;
					ret = true;
				}
				else
				{
					free(file.name);
				}
			}
			break;
		}

		case FILE_ACTION_REMOVED:
		{
			wchar_t* name = malloc(fni->FileNameLength + sizeof(wchar_t));
			if (name)
			{
				memcpy(name, fni->FileName, fni->FileNameLength);
				name[fni->FileNameLength / sizeof(wchar_t)] = 0;

				size_t pos = fileArrayFind(fileArray, name);
				if (pos != -1)
				{
					ret = true;
					if (fileArray->files[pos].name)
						free(fileArray->files[pos].name);

					for (size_t i = pos; i < fileArray->count - 1; ++i)
						fileArray->files[i] = fileArray->files[i + 1];
					--fileArray->count;
				}

				free(name);
			}
			break;
		}

		case FILE_ACTION_MODIFIED:
		{
			wchar_t* name = malloc(fni->FileNameLength + sizeof(wchar_t));
			if (name)
			{
				memcpy(name, fni->FileName, fni->FileNameLength);
				name[fni->FileNameLength / sizeof(wchar_t)] = 0;

				size_t pos = fileArrayFind(fileArray, name);
				if (pos != -1)
				{
					ret = true;
					getFileInfo(fileArray->files + pos, dir);
					if (sortMethod == SORT_SIZE || sortMethod == SORT_SIZE_INV || 
						sortMethod == SORT_CREATE || sortMethod == SORT_CREATE_INV || 
						sortMethod == SORT_WRITE || sortMethod == SORT_WRITE_INV)
						needsResort = true;
				}

				free(name);
			}
			break;
		}

		case FILE_ACTION_RENAMED_OLD_NAME:
		{
			// this always happens in the same buffer right before new
			wchar_t* name = malloc(fni->FileNameLength + sizeof(wchar_t));
			if (name)
			{
				memcpy(name, fni->FileName, fni->FileNameLength);
				name[fni->FileNameLength / sizeof(wchar_t)] = 0;

				replaceFileIndex = fileArrayFind(fileArray, name);

				free(name);
			}
			break;
		}

		case FILE_ACTION_RENAMED_NEW_NAME:
		{
			ret = true;
			if (replaceFileIndex != -1)
			{
				File* file = &fileArray->files[replaceFileIndex];
				if (file->name)
				{
					free(file->name);
					file->name = NULL;
				}
				file->name = malloc(fni->FileNameLength + sizeof(wchar_t));
				if (file->name)
				{
					memcpy(file->name, fni->FileName, fni->FileNameLength);
					file->name[fni->FileNameLength / sizeof(wchar_t)] = 0;
					needsResort = true;
				}
				replaceFileIndex = -1;
			}
			break;
		}
		}
	}

	if (needsResort)
		sortFileArray(fileArray, sortMethod);

	return ret;
}
