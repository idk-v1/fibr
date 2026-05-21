#include "filesystem.h"

#include <Windows.h>


#define EXTENSION_COLOR         255,  50, 150
#define ARCHIVE_COLOR             0, 255, 255
#define EXECUTABLE_COLOR        255,  31,  63
#define FILE_COLOR                0, 255, 191
#define FOLDER_COLOR              0, 159, 255
#define EMPTY_FOLDER_COLOR        0,  95, 191
#define HIGHLIGHT_COLOR         100,   0, 100
#define SORT_BAR_COLOR          255,   0, 150
#define BG_COLOR                 20,   0,  30
#define BG_ALT_COLOR             35,   0,  45


static void printU8(int num)
{
	char buf[4] = { 0 };
	buf[0] = '0' + num / 100 % 10;
	buf[1] = '0' + num / 10 % 10;
	buf[2] = '0' + num % 10;
	fputs(buf, stdout);
}

static void setFgColor(int r, int g, int b)
{
	fputs("\x1B[38;2;", stdout); 
	printU8(r);
	fputs(";", stdout); 
	printU8(g);
	fputs(";", stdout); 
	printU8(b);
	fputs("m", stdout); 
}
static void setBgColor(int r, int g, int b)
{
	fputs("\x1B[48;2;", stdout);
	printU8(r);
	fputs(";", stdout);
	printU8(g);
	fputs(";", stdout);
	printU8(b);
	fputs("m", stdout);
}

static void clearLn() { fputs("\x1B[0K", stdout); } // clear line after cursor
static void clearScr() { fputs("\x1B[0J", stdout); } // clear screen after cursor

static void setWindowTitle(const char* title) { 
	fputs("\x1B]0;", stdout);
	fputs(title, stdout);
	fputs("\x07", stdout); 
}

static void newScreenBuf() { fputs("\x1B[? 1 0 4 9 h", stdout); }
static void mainScreenBuf() { fputs("\x1B[? 1 0 4 9 l", stdout); }

static void setScrollLines(int line) 
{
	fputs("\x1B[", stdout);
	printU8(line);
	fputs(";r", stdout);
} // set scroll region
static void hideCursor() { fputs("\x1B[?25l", stdout); } // hides cursor
static void showCursor() { fputs("\x1B[?25h", stdout); } // hides cursor
static void resetCursor() { fputs("\x1B[;H", stdout); } // reset cursor


static wchar_t* initCurrentDir(void)
{
	int numArgs;
	wchar_t** args = CommandLineToArgvW(GetCommandLineW(), &numArgs);
	if (numArgs > 1) // Only need 1 extra optional for starting dir
	{
		size_t pathLen = wcslen(args[1]);
		wchar_t* dir = malloc(sizeof(wchar_t) * (pathLen + 1 + 4));
		if (!dir)
			return NULL;

		// strcpy, but replace '/' with '\\'
		for (size_t i = 0; i <= pathLen; ++i)
		{
			if (args[1][i] == L'/')
				dir[i + 4] = L'\\';
			else
				dir[i + 4] = args[1][i];
		}
		dir[0] = L'\\';
		dir[1] = L'\\';
		dir[2] = L'?';
		dir[3] = L'\\';

		LocalFree(args);
		return dir;
	}
	else
	{
		LocalFree(args);
		return getCurrentDir();
	}
}

static void enableVT()
{
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD mode = 0;
	GetConsoleMode(hConsole, &mode);
	SetConsoleMode(hConsole, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

	hConsole = GetStdHandle(STD_INPUT_HANDLE);
	mode = 0;
	GetConsoleMode(hConsole, &mode);
	SetConsoleMode(hConsole, mode | ENABLE_VIRTUAL_TERMINAL_INPUT);
	
	newScreenBuf();
	setWindowTitle("FiBr File Browser 0.1.9");
}

static void resetTerminal()
{
	fputs("\x1B[!p", stdout);
	mainScreenBuf();

	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	DWORD mode = 0;
	GetConsoleMode(hConsole, &mode);
	SetConsoleMode(hConsole, mode & ~ENABLE_VIRTUAL_TERMINAL_PROCESSING);

	hConsole = GetStdHandle(STD_INPUT_HANDLE);
	mode = 0;
	GetConsoleMode(hConsole, &mode);
	SetConsoleMode(hConsole, mode & ~ENABLE_VIRTUAL_TERMINAL_INPUT);
	
	FlushConsoleInputBuffer(hConsole);
}



static void rainbowPrintPath(const wchar_t* path, int consoleW)
{
	size_t len = wcslen(path);

	size_t sepCount = 0;
	int start = 0;

	setBgColor(BG_COLOR);

	if (len >= consoleW)
	{
		start = (int)len - consoleW + 1 + 3;
		for (size_t i = 0; i < start; ++i)
			if (path[i] == L'\\')
				++sepCount;

		setFgColor(255, 255, 255);
	}

	switch (sepCount % 12)
	{
	case 0: setFgColor(255, 0, 0); break;
	case 1: setFgColor(255, 127, 0); break;
	case 2: setFgColor(255, 255, 0); break;
	case 3: setFgColor(127, 255, 0); break;
	case 4: setFgColor(0, 255, 0); break;
	case 5: setFgColor(0, 255, 127); break;
	case 6: setFgColor(0, 255, 255); break;
	case 7: setFgColor(0, 127, 255); break;
	case 8: setFgColor(0, 0, 255); break;
	case 9: setFgColor(127, 0, 255); break;
	case 10:setFgColor(255, 0, 255); break;
	case 11:setFgColor(255, 0, 127); break;
	}
	for (size_t i = start; i < len; ++i)
	{
		if (path[i] == L'\\')
		{
			++sepCount;
			setFgColor(255, 255, 255);
			putwchar(L'\\');

			switch (sepCount % 12)
			{
			case 0: setFgColor(255, 0, 0); break;
			case 1: setFgColor(255, 127, 0); break;
			case 2: setFgColor(255, 255, 0); break;
			case 3: setFgColor(127, 255, 0); break;
			case 4: setFgColor(0, 255, 0); break;
			case 5: setFgColor(0, 255, 127); break;
			case 6: setFgColor(0, 255, 255); break;
			case 7: setFgColor(0, 127, 255); break;
			case 8: setFgColor(0, 0, 255); break;
			case 9: setFgColor(127, 0, 255); break;
			case 10:setFgColor(255, 0, 255); break;
			case 11:setFgColor(255, 0, 127); break;
			}
		}
		else
		{
			putwchar(path[i]);
		}
	}
}

static void printFileName(const wchar_t* file, size_t maxNameLen)
{
	if ((int)maxNameLen < 0)
		maxNameLen = 0;

	size_t len = wcslen(file);

	size_t extPos = -1;
	for (int i = (int)len - 1; i >= 0; --i)
		if (file[i] == L'.')
		{
			extPos = i;
			break;
		}

	if (len > maxNameLen)
	{
		size_t maxNameLenExt = (maxNameLen >= 3 ? maxNameLen - 3 : 0);
		if (extPos != -1) // Has extension
		{
			// Try to always print extension
			size_t extLen = len - extPos;
			if (extLen > maxNameLen) // This is a waste of time, unusable
			{
				setFgColor(EXTENSION_COLOR);
				for (int i = (int)extPos; i < (int)maxNameLenExt; ++i)
					putwchar(file[i]);
				putwchar(L'.');
				putwchar(L'.');
				putwchar(L'.');
			}
			else // print "thing....ext"
			{
				for (int i = 0; i < (int)(maxNameLenExt - extLen); ++i)
					putwchar(file[i]);
				// bug "thing.txt" becomes "....txt" when not enough space
				// but sometimes the "..." uses more space
				// this only happens in an unusable amount of space, ignoring
				putwchar(L'.');
				putwchar(L'.');
				putwchar(L'.');
				setFgColor(EXTENSION_COLOR);
				for (size_t i = extPos; i < len; ++i)
					putwchar(file[i]);
			}
		}
		else // No extension
		{
			for (size_t i = 0; i < maxNameLenExt; ++i)
				putwchar(file[i]);
			putwchar(L'.');
			putwchar(L'.');
			putwchar(L'.');
		}
	}
	else // Normal
	{
		if (extPos != -1)
		{
			for (size_t i = 0; i < extPos; ++i)
				putwchar(file[i]);
			setFgColor(EXTENSION_COLOR);
			for (size_t i = extPos; i < len; ++i)
				putwchar(file[i]);
		}
		else
		{
			for (size_t i = 0; i < len; ++i)
				putwchar(file[i]);
		}
		for (int i = (int)len; i < (int)maxNameLen; ++i)
			putwchar(L' ');
	}
}



static void printFileArray(FileArray fileArray, int highlight, int start, int end, int maxNameLen)
{
	for (size_t i = start; i < end; ++i)
	{
		File file = fileArray.files[fileArray.count - 1 - i];
		if (i == highlight)
			setBgColor(HIGHLIGHT_COLOR);
		else
		{
			if (i % 2)
				setBgColor(BG_COLOR);
			else
				setBgColor(BG_ALT_COLOR);
		}

		if (file.isFile)
		{
			if (file.isArchive)
			{
				setFgColor(ARCHIVE_COLOR);
				fputs("A ", stdout);
			}
			else if (file.isExec)
			{
				setFgColor(EXECUTABLE_COLOR);
				fputs("X ", stdout);
			}
			else
			{
				setFgColor(FILE_COLOR);
				fputs("| ", stdout);
			}

			printFileName(file.name, maxNameLen);
			//if (wcslen(file.name) > maxNameLen)
			//	wprintf(L"%-*.*s...| ", maxNameLen - 3, maxNameLen - 3, file.name);
			//else
			//	wprintf(L"%-*.*s| ", maxNameLen, maxNameLen, file.name);

			if (file.isArchive)
				setFgColor(ARCHIVE_COLOR);
			else if (file.isExec)
				setFgColor(EXECUTABLE_COLOR);
			else
				setFgColor(FILE_COLOR);

			int unit = 0;
			size_t size = file.size;
			while (size / 1000)
			{
				++unit;
				size /= 1000;
			}
			printf("| ");
			switch (unit % 7)
			{
			case 0: setFgColor(127, 255,   0); break;
			case 1: setFgColor(255, 255,   0); break;
			case 2: setFgColor(255, 127,   0); break;
			case 3: setFgColor(255,   0,   0); break;
			case 4: setFgColor(255,   0, 127); break;
			case 5: setFgColor(255,   0, 255); break;
			case 6: setFgColor(127,   0, 255); break;
			}
			printf("%3llu %cB", size, " KMGTPE"[unit]);
			if (file.isArchive)
				setFgColor(ARCHIVE_COLOR);
			else if (file.isExec)
				setFgColor(EXECUTABLE_COLOR);
			else
				setFgColor(FILE_COLOR);
			printf(" |");
		}
		else
		{
			if (file.size)
				setFgColor(FOLDER_COLOR);
			else
				setFgColor(EMPTY_FOLDER_COLOR);
			fputs("> ", stdout);
			if (wcslen(file.name) > maxNameLen)
				wprintf(L"%-*.*s...| ", maxNameLen - 3, maxNameLen - 3, file.name);
			else
				wprintf(L"%-*.*s| ", maxNameLen, maxNameLen, file.name);
			printf("%6llu |", file.size);
		}
		printf(" %04u/%02u/%02u %02u:%02u | %04u/%02u/%02u %02u:%02u |",
			file.createTime.year, file.createTime.month, file.createTime.day, file.createTime.hour, file.createTime.minute,
			file.writeTime.year, file.writeTime.month, file.writeTime.day, file.writeTime.hour, file.writeTime.minute);
		setBgColor(BG_COLOR);
		clearLn();
		fputs("\n", stdout);
	}
}

static void printDriveArray(DriveArray driveArray, int highlight, int start, int end, int maxNameLen)
{
	for (size_t i = start; i < end; ++i)
	{
		Drive drive = driveArray.drives[driveArray.count - 1 - i];
		if (i == highlight)
			setBgColor(HIGHLIGHT_COLOR);
		else
		{
			if (i % 2)
				setBgColor(BG_COLOR);
			else
				setBgColor(BG_ALT_COLOR);
		}

		setFgColor(FOLDER_COLOR);
		fputs("# ", stdout);
		size_t drNameLen = wcslen(drive.name);
		size_t drPathLen = wcslen(drive.path);
		wchar_t* name = malloc(sizeof(wchar_t) * (drNameLen + drPathLen + 3 + 1));
		if (name)
		{
			memcpy(name, drive.name, sizeof(wchar_t) * drNameLen);
			name[drNameLen + 0] = L' ';
			name[drNameLen + 1] = L'(';
			memcpy(name + drNameLen + 2, drive.path, sizeof(wchar_t) * drPathLen);
			name[drNameLen + 2 + drPathLen + 0] = L')';
			name[drNameLen + 2 + drPathLen + 1] = 0;

			if (wcslen(name) > maxNameLen)
				wprintf(L"%-*.*s...| ", maxNameLen - 3, maxNameLen - 3, name);
			else
				wprintf(L"%-*.*s| ", maxNameLen, maxNameLen, name);

			free(name);
		}
		else
		{
			if (wcslen(drive.name) > maxNameLen)
				wprintf(L"%-*.*s...| ", maxNameLen - 3, maxNameLen - 3, drive.name);
			else
				wprintf(L"%-*.*s| ", maxNameLen, maxNameLen, drive.name);
		}
		
		int capUnit = 0;
		size_t cap = drive.capacity;
		while (cap / 1000)
		{
			++capUnit;
			cap /= 1000;
		}
		int freeUnit = 0;
		size_t free = drive.free;
		while (free / 1000)
		{
			++freeUnit;
			free /= 1000;
		}

		printf("%3llu %cB free of %3llu %cB |", free, " KMGTPE"[freeUnit], cap, " KMGTPE"[capUnit]);
		
		setBgColor(BG_COLOR);
		clearLn();
		fputs("\n", stdout);
	}
}

static bool getConsoleSize(int* w, int* h)
{
	static int sw = 0, sh = 0;

	CONSOLE_SCREEN_BUFFER_INFO csbi;
	GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
	int nw = csbi.srWindow.Right - csbi.srWindow.Left + 1;
	int nh = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

	bool ret = (nw != *w || nh != *h);

	*w = nw;
	*h = nh;

	sw = *w;
	sh = *h;

	return ret;
}


static void display(FileArray* fileArray, DriveArray* driveArray, bool isFileArray, 
	int sortMethod, const wchar_t* currentDir, int highlight, int consoleW, int consoleH)
{
	int maxNameLen;
	size_t dataCount;

	if (isFileArray)
	{
		maxNameLen = consoleW - 51;
		dataCount = fileArray->count;
	}
	else
	{
		maxNameLen = consoleW - 28;
		dataCount = driveArray->count;
	}


	int start = highlight - (consoleH - 2) / 2;
	int end = highlight + ((consoleH - 2) - (consoleH - 2) / 2);
	if (start < 0)
	{
		end -= start + 1;
		start = 0;
	}
	if (end > dataCount)
		end = (int)dataCount;
	if (end - start > consoleH - 3)
		end = consoleH - 3 + start;

	setScrollLines(3);
	hideCursor();
	resetCursor();

	size_t pathLen = wcslen(currentDir + 4);
	if (isFileArray)
	{
		rainbowPrintPath(currentDir + 4, consoleW);

		setFgColor(SORT_BAR_COLOR);
		setBgColor(BG_COLOR);
		clearLn();
		fputs("\n", stdout);

		printf("| %-*s%c%c| Size  %c| Create          %c| Write           %c|", maxNameLen - 2, "Name",
			(sortMethod == SORT_TYPE || sortMethod == SORT_TYPE_INV ? 'T' : ' '),
			(sortMethod == SORT_NAME || sortMethod == SORT_TYPE ? '^' : (sortMethod == SORT_NAME_INV || sortMethod == SORT_TYPE_INV ? 'v' : ' ')),
			(sortMethod == SORT_SIZE ? '^' : (sortMethod == SORT_SIZE_INV ? 'v' : ' ')),
			(sortMethod == SORT_CREATE ? '^' : (sortMethod == SORT_CREATE_INV ? 'v' : ' ')),
			(sortMethod == SORT_WRITE ? '^' : (sortMethod == SORT_WRITE_INV ? 'v' : ' '))
		);
	}
	else
	{
		setFgColor(SORT_BAR_COLOR);
		setBgColor(BG_COLOR);
		wprintf(L"Drives");

		clearLn();
		fputs("\n", stdout);

		printf("| %-*s%c%c| Size                 %c|", maxNameLen - 2, "Name",
			(sortMethod == SORT_LETTER || sortMethod == SORT_LETTER_INV ? 'L' : ' '),
			(sortMethod == SORT_NAME || sortMethod == SORT_LETTER ? '^' : (sortMethod == SORT_NAME_INV || sortMethod == SORT_LETTER_INV ? 'v' : ' ')),
			(sortMethod == SORT_SIZE ? '^' : (sortMethod == SORT_SIZE_INV ? 'v' : ' '))
		);
	}

	clearLn();
	fputs("\n", stdout);

	if (isFileArray)
		printFileArray(*fileArray, highlight, start, end, maxNameLen);
	else
		printDriveArray(*driveArray, highlight, start, end, maxNameLen);

	setBgColor(BG_COLOR);
	clearScr();
}



typedef struct DirStack
{
	wchar_t** data;
	size_t size;
} DirStack;

static void dirStackPush(DirStack* stack, const wchar_t* dir)
{
	wchar_t** temp = realloc(stack->data, sizeof(wchar_t*) * (stack->size + 1));
	if (temp)
		stack->data = temp;
	else // not adding to stack will cause more problems than forgetting where you were
	{
		for (size_t i = 0; i < stack->size; ++i)
			free(stack->data[i]);
		free(stack->data);
		stack->data = NULL;
		stack->size = 0;
		return;
	}

	size_t len = wcslen(dir);
	wchar_t* add = malloc(sizeof(wchar_t) * (len + 1));
	if (add)
	{
		memcpy(add, dir, sizeof(wchar_t) * (len + 1));
		stack->data[stack->size] = add;
		++stack->size;
	}
	else // not adding to stack will cause more problems than forgetting where you were
	{
		for (size_t i = 0; i < stack->size; ++i)
			free(stack->data[i]);
		free(stack->data);
		stack->data = NULL;
		stack->size = 0;
	}
}

static wchar_t* dirStackPop(DirStack* stack)
{
	if (stack->size)
	{
		--stack->size;
		return stack->data[stack->size];
	}
	return NULL;
}

static void dirStackInit(DirStack* stack, const wchar_t* path)
{
	stack->data = NULL;
	stack->size = 0;

	size_t len = wcslen(path);
	wchar_t* dupPath = malloc(sizeof(wchar_t) * (len + 2));
	if (dupPath)
	{
		memcpy(dupPath, path, sizeof(wchar_t) * len);
		dupPath[len] = L'\\';
		dupPath[len + 1] = 0;

		size_t start = 4;
		for (size_t i = 4; i <= len; ++i)
		{
			if (dupPath[i] == L'\\')
			{
				dupPath[i] = 0;
				dirStackPush(stack, dupPath + start);
				dupPath[i] = L'\\';

				start = i + 1;
			}
		}

		free(dupPath);
	}
}


static void resortDir(const wchar_t* currentDir, FileArray* fileArray, 
	DriveArray* driveArray, bool isFileArray, int sortMethod, int* highlight, wchar_t** retDir)
{
	if (isFileArray)
	{
		if (fileArray->count)
		{
			wchar_t* temp = fileArray->files[fileArray->count - 1 - *highlight].name;
			sortFileArray(fileArray, sortMethod);
			if (*retDir)
			{
				for (int i = 0; i < fileArray->count; ++i)
				{
					if (wcscmp(fileArray->files[i].name, *retDir) == 0)
					{
						*highlight = (int)fileArray->count - 1 - i;
						break;
					}
				}

				free(*retDir);
				*retDir = NULL;
			}
			else
			{
				for (int i = 0; i < fileArray->count; ++i)
				{
					if (wcscmp(fileArray->files[i].name, temp) == 0)
					{
						*highlight = (int)fileArray->count - 1 - i;
						break;
					}
				}
			}
		}
	}
	else
	{
		if (driveArray->count)
		{
			wchar_t* temp = driveArray->drives[driveArray->count - 1 - *highlight].name;
			sortDriveArray(driveArray, sortMethod);
			if (*retDir)
			{
				for (int i = 0; i < driveArray->count; ++i)
				{
					if (wcscmp(driveArray->drives[i].path, *retDir) == 0)
					{
						*highlight = (int)driveArray->count - 1 - i;
						break;
					}
				}

				free(*retDir);
				*retDir = NULL;
			}
			else
			{
				for (int i = 0; i < driveArray->count; ++i)
				{
					if (wcscmp(driveArray->drives[i].name, temp) == 0)
					{
						*highlight = (int)driveArray->count - 1 - i;
						break;
					}
				}
			}
		}
	}

	int consoleW = 0, consoleH = 0;
	getConsoleSize(&consoleW, &consoleH);
	display(fileArray, driveArray, isFileArray, sortMethod, currentDir, *highlight, consoleW, consoleH);
}

static void changedDir(const wchar_t* currentDir, FileArray* fileArray, 
	DriveArray* driveArray, bool* isFileArray, bool loadSubdirs, 
	int* sortMethod, int* highlight, size_t* dataCount, wchar_t** retDir)
{
	*highlight = 0;

	if (*isFileArray)
		freeFileArray(fileArray);
	else
		freeDriveArray(driveArray);

	if (wcslen(currentDir) > 4)
	{
		*fileArray = getFilesInDir(currentDir, loadSubdirs);
		*isFileArray = true;

		*dataCount = fileArray->count;

		// Needs to be sorted for highlight to be correct
		if (*sortMethod == SORT_LETTER || *sortMethod == SORT_LETTER_INV)
			*sortMethod = SORT_NAME;
	}
	else
	{
		*driveArray = getDrives();
		*isFileArray = false;

		*dataCount = driveArray->count;

		// Needs to be sorted for highlight to be correct
		if (*sortMethod == SORT_TYPE   || *sortMethod == SORT_TYPE_INV ||
			*sortMethod == SORT_CREATE || *sortMethod == SORT_CREATE_INV ||
			*sortMethod == SORT_WRITE  || *sortMethod == SORT_WRITE_INV)
			*sortMethod = SORT_NAME;
	}

	resortDir(currentDir, fileArray, driveArray, *isFileArray, *sortMethod, highlight, retDir);
}


int main()
{
	// read last size from file
	// be like explorer or notepad

	// start in dir specified by parameter

	enableVT();

	HWND consoleWnd = GetConsoleWindow();

	wchar_t* currentDir = initCurrentDir();

	DirStack dirStack = { 0 };
	dirStackInit(&dirStack, currentDir);

	size_t dataCount = 0;
	FileArray fileArray = { 0 };
	DriveArray driveArray = { 0 };
	bool isFileArray = wcslen(currentDir) > 4;

	int highlight = 0;

	int keyboard[256] = { 0 };
	int keyRepWait = 6;

	bool dirChanged = true;
	bool reprint = true;
	bool resort = true;
	wchar_t* retDir = NULL;
	bool loadSubdirs = true;

	int sortMethod = SORT_NAME;

	bool running = true;
	while (running)
	{
		int consoleW, consoleH;
		if (getConsoleSize(&consoleW, &consoleH))
		{
			reprint = true;
		}

		if (dirChanged)
		{
			dirChanged = false;
			resort = false;
			reprint = false;
			changedDir(currentDir, &fileArray, &driveArray, &isFileArray, loadSubdirs, &sortMethod, &highlight, &dataCount, &retDir);
		}
		if (resort)
		{
			resort = false;
			reprint = false;
			resortDir(currentDir, &fileArray, &driveArray, isFileArray, sortMethod, &highlight, &retDir);
		}
		if (reprint)
		{
			reprint = false;
			display(&fileArray, &driveArray, isFileArray, sortMethod, currentDir, highlight, consoleW, consoleH);
		}

		if (consoleWnd == GetForegroundWindow())
		{
			for (int i = 0; i < 256; ++i)
			{
				if (GetKeyState(i) & 0x8000)
					++keyboard[i];
				else
					keyboard[i] = 0;
			}

			if (keyboard[VK_ESCAPE])
				running = false;

			if (keyboard[VK_DOWN] && (keyboard[VK_DOWN] + keyRepWait - 2) / keyRepWait != 1)
			{
				if (highlight + 1ull < dataCount)
				{
					if (keyboard[VK_CONTROL])
						highlight = min((int)dataCount - 1, highlight + 5);
					else
						++highlight;
					reprint = true;
				}
			}
			if (keyboard[VK_UP] && (keyboard[VK_UP] + keyRepWait - 2) / keyRepWait != 1)
			{
				if (highlight > 0)
				{
					if (keyboard[VK_CONTROL])
						highlight = max(0, highlight - 5);
					else
						--highlight;
					reprint = true;
				}
			}
			if (keyboard[VK_RIGHT] == 1)
			{
				loadSubdirs = !keyboard[VK_CONTROL];
				if (isFileArray)
				{
					if (fileArray.count && !fileArray.files[fileArray.count - 1 - highlight].isFile)
					{
						dirChanged = true;
						bool error = false;
						currentDir = moveDirDown(currentDir, fileArray.files[fileArray.count - 1 - highlight].name, &error);
						dirStackPush(&dirStack, fileArray.files[fileArray.count - 1 - highlight].name);
					}
				}
				else
				{
					if (driveArray.count)
					{
						dirChanged = true;
						bool error = false;
						currentDir = moveDirDown(currentDir, driveArray.drives[driveArray.count - 1 - highlight].path, &error);
						dirStackPush(&dirStack, driveArray.drives[driveArray.count - 1 - highlight].path);
					}
				}
			}
			if (keyboard[VK_LEFT] == 1)
			{
				loadSubdirs = !keyboard[VK_CONTROL];
				dirChanged = true;

				bool error = false;
				currentDir = moveDirUp(currentDir, &error);
				if (error)
					dirChanged = false;

				retDir = dirStackPop(&dirStack);
			}

			if (keyboard['N'] == 1)
			{
				if (sortMethod == SORT_NAME)
					sortMethod = SORT_NAME_INV;
				else
					sortMethod = SORT_NAME;
				resort = true;
			}
			if (keyboard['T'] == 1)
			{
				if (sortMethod == SORT_TYPE)
					sortMethod = SORT_TYPE_INV;
				else
					sortMethod = SORT_TYPE;
				resort = true;
			}
			if (keyboard['S'] == 1)
			{
				if (sortMethod == SORT_SIZE)
					sortMethod = SORT_SIZE_INV;
				else
					sortMethod = SORT_SIZE;
				resort = true;
			}
			if (keyboard['C'] == 1)
			{
				if (sortMethod == SORT_CREATE)
					sortMethod = SORT_CREATE_INV;
				else
					sortMethod = SORT_CREATE;
				resort = true;
			}
			if (keyboard['W'] == 1)
			{
				if (sortMethod == SORT_WRITE)
					sortMethod = SORT_WRITE_INV;
				else
					sortMethod = SORT_WRITE;
				resort = true;
			}
			if (keyboard['L'] == 1)
			{
				if (sortMethod == SORT_LETTER)
					sortMethod = SORT_LETTER_INV;
				else
					sortMethod = SORT_LETTER;
				resort = true;
			}

		}

		Sleep(50);
	}

	if (retDir)
		free(retDir);
	for (size_t i = 0; i < dirStack.size; ++i)
		free(dirStack.data[i]);
	free(dirStack.data);
	
	if (currentDir)
		free(currentDir);

	if (isFileArray)
		freeFileArray(&fileArray);
	else
		freeDriveArray(&driveArray);

	resetTerminal();

	return 0;
}