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

DriveArray getDrives()
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
	size_t cap = bitcount;
	array.drives = malloc(sizeof(Drive) * cap);
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
			if (array.count + 1 > cap)
			{
				cap *= 2;
				Drive* temp = realloc(array.drives, sizeof(Drive) * cap);
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
				len = volNameSize - 2; // size includes NULL and trailing slash
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
	}
}

FileArray getDrivesAsFileArray()
{
	DriveArray drives = getDrives();

	FileArray array = { 0 };
	array.count = drives.count;
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
	size_t cap = 16;

	array.files = malloc(sizeof(File) * cap);
	if (!array.files)
	{
		FindClose(hFind);
		return (FileArray) { 0 };
	}


	while (FindNextFileW(hFind, &findData))
	{
		if (array.count + 1 > cap)
		{
			cap *= 2;
			File* temp = realloc(array.files, sizeof(File) * cap);
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
	case SORT_PATH: cmpFnc = cmpDrivesPathTop; break;
	case SORT_PATH_INV: cmpFnc = cmpDrivesPathInvTop; break;
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
