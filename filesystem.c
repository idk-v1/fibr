#include "filesystem.h"

#include <Windows.h>

Date dateFromFiletime(FILETIME ft)
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

FileArray getFilesInDir(const wchar_t* path, bool subdirCount)
{
	if (!path)
		return (FileArray){0};

	size_t pathLen = lstrlenW(path);
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

	
	WIN32_FIND_DATAW findData;
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
	size_t count = getFileCountInDir(path);

	array.files = malloc(sizeof(File) * count);
	if (!array.files)
	{
		FindClose(hFind);
		return (FileArray) { 0 };
	}


	while (FindNextFileW(hFind, &findData))
	{
		// maybe some file is added in an inconvientient time
		if (array.count + 1 > count)
		{
			count += 5;
			File* temp = realloc(array.files, sizeof(File) * count);
			if (temp)
				array.files = temp;
			else break; // be happy with what we've got
		}

		File file;
		file.isFile = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0;

		size_t nameLen = lstrlenW(findData.cFileName);
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
			LARGE_INTEGER fileSize;
			fileSize.HighPart = findData.nFileSizeHigh;
			fileSize.LowPart = findData.nFileSizeLow;
			file.size = fileSize.QuadPart;
		}
		else if (subdirCount)
		{
			wchar_t* subdir = strSubdir(path, file.name);
			file.size = getFileCountInDir(subdir);
			free(subdir);
		}
		else file.size = 0;

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

	size_t pathLen = lstrlenW(path);
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


	WIN32_FIND_DATAW findData;
	HANDLE hFind = FindFirstFileExW(wildPath, FindExInfoBasic, &findData,
		FindExSearchNameMatch, NULL, FIND_FIRST_EX_LARGE_FETCH);
	if (hFind == INVALID_HANDLE_VALUE)
	{
		free(wildPath);
		return 0;
	}
	free(wildPath);
	// the fake directories "." and ".." always appear first
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

	size_t dirLen = lstrlenW(dir);
	size_t subLen = lstrlenW(sub);

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
				if (fileArray->files[i].name)
					free(fileArray->files[i].name);
			free(fileArray->files);
			fileArray->files = NULL;
		}
		fileArray->count = 0;
	}
}


int cmpFilesName(const wchar_t* a, const wchar_t* b)
{
	return lstrcmpW(a, b);
}

int cmpFilesSize(size_t a, size_t b)
{
	return (a > b ? 1 : -1);
}

int cmpFilesDate(Date a, Date b)
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

int cmpFilesType(const wchar_t* a, const wchar_t* b)
{
	size_t aLen = lstrlenW(a);
	size_t bLen = lstrlenW(b);

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

int cmpFilesNameTop(File const* a, File const* b)
{
	if (!a->isFile && b->isFile)
		return 1;
	if (a->isFile && !b->isFile)
		return -1;

	return -cmpFilesName(a->name, b->name);
}

int cmpFilesNameInvTop(File const* a, File const* b)
{
	if (!a->isFile && b->isFile)
		return 1;
	if (a->isFile && !b->isFile)
		return -1;

	return cmpFilesName(a->name, b->name);
}

int cmpFilesSizeTop(File const* a, File const* b)
{
	if (!a->isFile && b->isFile)
		return 1;
	if (a->isFile && !b->isFile)
		return -1;

	int ret = -cmpFilesSize(a->size, b->size);
	if (ret) return ret;

	return -cmpFilesName(a->name, b->name);
}

int cmpFilesSizeInvTop(File const* a, File const* b)
{
	if (!a->isFile && b->isFile)
		return 1;
	if (a->isFile && !b->isFile)
		return -1;

	int ret = cmpFilesSize(a->size, b->size);
	if (ret) return ret;

	return -cmpFilesName(a->name, b->name);
}

int cmpFilesCreateTop(File const* a, File const* b)
{
	if (!a->isFile && b->isFile)
		return 1;
	if (a->isFile && !b->isFile)
		return -1;

	int ret = -cmpFilesDate(a->createTime, b->createTime);
	if (ret) return ret;

	return -cmpFilesName(a->name, b->name);
}

int cmpFilesCreateInvTop(File const* a, File const* b)
{
	if (!a->isFile && b->isFile)
		return 1;
	if (a->isFile && !b->isFile)
		return -1;

	int ret = cmpFilesDate(a->createTime, b->createTime);
	if (ret) return ret;

	return -cmpFilesName(a->name, b->name);
}

int cmpFilesWriteTop(File const* a, File const* b)
{
	if (!a->isFile && b->isFile)
		return 1;
	if (a->isFile && !b->isFile)
		return -1;

	int ret = -cmpFilesDate(a->writeTime, b->writeTime);
	if (ret) return ret;

	return -cmpFilesName(a->name, b->name);
}

int cmpFilesWriteInvTop(File const* a, File const* b)
{
	if (!a->isFile && b->isFile)
		return 1;
	if (a->isFile && !b->isFile)
		return -1;

	int ret = cmpFilesDate(a->writeTime, b->writeTime);
	if (ret) return ret;

	return -cmpFilesName(a->name, b->name);
}

int cmpFilesTypeTop(File const* a, File const* b)
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

int cmpFilesTypeInvTop(File const* a, File const* b)
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


void sortFileArray(FileArray fileArray, int option)
{
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
		qsort(fileArray.files, fileArray.count, sizeof(File), cmpFnc);
}

wchar_t* moveDirUp(wchar_t* dir)
{
	if (!dir)
		return NULL;
	size_t len = lstrlenW(dir);

	size_t pos = len - 1;

	for (size_t i = len - 1; i >= 4; --i)
		if (dir[i] == L'\\')
		{
			pos = i;
			break;
		}

	if (pos + 1 == len)
	{
		return dir;
	}
	dir[pos] = 0;
	return dir;
}

wchar_t* moveDirDown(wchar_t* dir, const wchar_t* subdir)
{
	if (!dir)
		return NULL;
	if (!subdir)
		return dir;

	wchar_t* newDir = strSubdir(dir, subdir);
	free(dir);
	return newDir;
}
