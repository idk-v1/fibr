#include "filesystem.h"

#include <Windows.h>

#define SETFGCOLOR(r, g, b) fputs("\x1B[38;2;"#r";"#g";"#b"", stdout)
#define SETBGCOLOR(r, g, b) fputs("\x1B[48;2;"#r";"#g";"#b"", stdout)

#define CLEARLN fputs("\x1B[0K", stdout); // clear line after cursor
#define CLEARSCR fputs("\x1B[0J", stdout); // clear screen after cursor

#define SETWINDOWTITLE(title) fputs("\x1B]0;"#title"\x07", stdout)

#define NEWSCREENBUF fputs("\x1B[? 1 0 4 9 h", stdout);
#define MAINSCREENBUF fputs("\x1B[? 1 0 4 9 l", stdout);
#define RESET fputs("\x1B[!p", stdout);

#define SETSCROLL(line) fputs("\x1B["#line";r", stdout); // set scroll region
#define HIDECURSOR fputs("\x1B[?25l", stdout); // hides cursor
#define SHOWCURSOR fputs("\x1B[?25h", stdout); // hides cursor
#define RESETCURSOR fputs("\x1B[;H", stdout); // reset cursor


static void initCurrentDir(wchar_t** currentDir)
{
	wchar_t* dir = NULL;
	int numArgs;
	wchar_t** args = CommandLineToArgvW(GetCommandLineW(), &numArgs);
	if (numArgs > 1) // Only need 1 extra optional for starting dir
	{
		size_t pathLen = wcslen(args[1]);
		dir = malloc(sizeof(wchar_t) * (pathLen + 1 + 4));
		if (!dir)
			return;

		for (size_t i = 0; i <= pathLen; ++i)
		{
			if (args[1][i] == L'/')
				dir[i + 4] = L'\\';
			else
				dir[i + 4] = args[1][i];
		}
	}
	else
	{
		size_t pathLen = GetCurrentDirectoryW(0, NULL); // returns size needed including null
		dir = malloc(sizeof(wchar_t) * (pathLen + 4));
		if (!dir)
			return;

		GetCurrentDirectoryW((DWORD)pathLen, dir + 4);
	}

	dir[0] = L'\\';
	dir[1] = L'\\';
	dir[2] = L'?';
	dir[3] = L'\\';
	*currentDir = dir;
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
	
	fputs("\x1B[? 1 0 4 9 h", stdout); // new screen buffer

	fputs("\x1B]0;FiBr File Browser 0.1.7\x07", stdout);
}

static void resetTerminal()
{
	fputs("\x1B[!p", stdout); // reset
	fputs("\x1B[? 1 0 4 9 l", stdout); // main screen buffer

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

	fputs("\x1B[40m", stdout); // bg black

	if (len >= consoleW)
	{
		start = len - consoleW + 1 + 3;
		for (size_t i = 0; i < start; ++i)
			if (path[i] == L'\\')
				++sepCount;

		fputs("\x1B[97m...", stdout); // fg bright white
	}

	switch (sepCount % 12)
	{
	case 0:	fputs("\x1B[31m", stdout); break; // fg red
	case 1:	fputs("\x1B[91m", stdout); break; // fg bright red
	case 2:	fputs("\x1B[33m", stdout); break; // fg yellow
	case 3:	fputs("\x1B[93m", stdout); break; // fg bright yellow
	case 4:	fputs("\x1B[92m", stdout); break; // fg bright green
	case 5:	fputs("\x1B[32m", stdout); break; // fg green
	case 6:	fputs("\x1B[96m", stdout); break; // fg bright cyan
	case 7:	fputs("\x1B[36m", stdout); break; // fg cyan
	case 8:	fputs("\x1B[94m", stdout); break; // fg bright blue
	case 9:	fputs("\x1B[34m", stdout); break; // fg blue
	case 10:fputs("\x1B[35m", stdout); break; // fg magenta
	case 11:fputs("\x1B[95m", stdout); break; // fg bright magenta
	}
	for (size_t i = start; i < len; ++i)
	{
		if (path[i] == L'\\')
		{
			++sepCount;
			fputs("\x1B[97m\\", stdout); // fg bright white
			switch (sepCount % 12)
			{
			case 0:	fputs("\x1B[31m", stdout); break; // fg red
			case 1:	fputs("\x1B[91m", stdout); break; // fg bright red
			case 2:	fputs("\x1B[33m", stdout); break; // fg yellow
			case 3:	fputs("\x1B[93m", stdout); break; // fg bright yellow
			case 4:	fputs("\x1B[92m", stdout); break; // fg bright green
			case 5:	fputs("\x1B[32m", stdout); break; // fg green
			case 6:	fputs("\x1B[96m", stdout); break; // fg bright cyan
			case 7:	fputs("\x1B[36m", stdout); break; // fg cyan
			case 8:	fputs("\x1B[94m", stdout); break; // fg bright blue
			case 9:	fputs("\x1B[34m", stdout); break; // fg blue
			case 10:fputs("\x1B[35m", stdout); break; // fg magenta
			case 11:fputs("\x1B[95m", stdout); break; // fg bright magenta
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
	if ((long long)maxNameLen < 0)
		maxNameLen = 0;

	size_t len = wcslen(file);

	size_t extPos = -1;
	for (int i = len - 1; i >= 0; --i)
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
				fputs("\x1B[38;2;255;0;150m", stdout); // set fg color to hot pink
				for (int i = extPos; i < maxNameLenExt; ++i)
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
				fputs("\x1B[38;2;255;0;150m", stdout); // set fg color to hot pink
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
			fputs("\x1B[38;2;255;0;150m", stdout); // set fg color to hot pink
			for (size_t i = extPos; i < len; ++i)
				putwchar(file[i]);
		}
		else
		{
			for (size_t i = 0; i < len; ++i)
				putwchar(file[i]);
		}
		for (int i = len; i < maxNameLen; ++i)
			putwchar(L' ');
	}
}



static void printFileArray(FileArray fileArray, int highlight, int start, int end, int maxNameLen)
{
	for (size_t i = start; i < end; ++i)
	{
		File file = fileArray.files[fileArray.count - 1 - i];
		if (i == highlight)
		{
			fputs("\x1B[45m", stdout); // set bg color to magenta
		}

		if (file.isFile)
		{
			if (file.isArchive)
			{
				fputs("\x1B[96m", stdout); // set fg color to cyan
				fputs("v ", stdout);
			}
			else
			{
				fputs("\x1B[97m", stdout); // set fg color to white
				fputs("| ", stdout);
			}

			printFileName(file.name, maxNameLen);
			//if (wcslen(file.name) > maxNameLen)
			//	wprintf(L"%-*.*s...| ", maxNameLen - 3, maxNameLen - 3, file.name);
			//else
			//	wprintf(L"%-*.*s| ", maxNameLen, maxNameLen, file.name);

			if (file.isArchive)
				fputs("\x1B[96m", stdout); // set fg color to cyan
			else
				fputs("\x1B[97m", stdout); // set fg color to white

			int unit = 0;
			size_t size = file.size;
			while (size / 1000)
			{
				++unit;
				size /= 1000;
			}
			printf("| %3llu %cB |", size, " KMGTPE"[unit]);
		}
		else
		{
			fputs("\x1B[36m", stdout); // set fg color to blue (dark cyan)
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
		fputs("\x1B[40m", stdout); // set bg color to black
		fputs("\x1B[0K", stdout); // clear line after cursor
		fputs("\n", stdout);
	}
}

static void printDriveArray(DriveArray driveArray, int highlight, int start, int end, int maxNameLen)
{
	for (size_t i = start; i < end; ++i)
	{
		Drive drive = driveArray.drives[driveArray.count - 1 - i];
		if (i == highlight)
		{
			fputs("\x1B[45m", stdout); // set bg color to magenta
		}

		fputs("\x1B[36m", stdout); // set fg color to blue (dark cyan)
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
		
		fputs("\x1B[40m", stdout); // set bg color to black
		fputs("\x1B[0K", stdout); // clear line after cursor
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

	fputs("\x1B[3;r", stdout); // set scroll region
	fputs("\x1B[?25l", stdout); // hides cursor
	fputs("\x1B[;H", stdout); // reset cursor

	size_t pathLen = wcslen(currentDir + 4);
	if (isFileArray)
	{
		rainbowPrintPath(currentDir + 4, consoleW);

		fputs("\x1B[40m\x1B[95m", stdout); // fg magenta, bg black
		fputs("\x1B[0K\n", stdout); // clear line after cursor

		printf("| %-*s%c%c| Size  %c| Create          %c| Write           %c|", maxNameLen - 2, "Name",
			(sortMethod == SORT_TYPE || sortMethod == SORT_TYPE_INV ? 't' : ' '),
			(sortMethod == SORT_NAME || sortMethod == SORT_TYPE ? '^' : (sortMethod == SORT_NAME_INV || sortMethod == SORT_TYPE_INV ? 'v' : ' ')),
			(sortMethod == SORT_SIZE ? '^' : (sortMethod == SORT_SIZE_INV ? 'v' : ' ')),
			(sortMethod == SORT_CREATE ? '^' : (sortMethod == SORT_CREATE_INV ? 'v' : ' ')),
			(sortMethod == SORT_WRITE ? '^' : (sortMethod == SORT_WRITE_INV ? 'v' : ' '))
		);
	}
	else
	{
		fputs("\x1B[40m\x1B[95m", stdout); // fg magenta, bg black
		wprintf(L"Drives");

		fputs("\x1B[0K\n", stdout); // clear line after cursor

		printf("| %-*s%c%c| Size                 %c|", maxNameLen - 2, "Name",
			(sortMethod == SORT_PATH || sortMethod == SORT_PATH_INV ? 'p' : ' '),
			(sortMethod == SORT_NAME || sortMethod == SORT_PATH ? '^' : (sortMethod == SORT_NAME_INV || sortMethod == SORT_PATH_INV ? 'v' : ' ')),
			(sortMethod == SORT_SIZE ? '^' : (sortMethod == SORT_SIZE_INV ? 'v' : ' '))
		);
	}

	fputs("\x1B[0K\n", stdout); // clear line after cursor

	if (isFileArray)
		printFileArray(*fileArray, highlight, start, end, maxNameLen);
	else
		printDriveArray(*driveArray, highlight, start, end, maxNameLen);

	fputs("\x1B[40m", stdout); // set bg color to black
	fputs("\x1B[0J", stdout); // clear screen after cursor
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


int main()
{
	// read last size from file
	// be like explorer or notepad

	// start in dir specified by parameter

	enableVT();

	HWND consoleWnd = GetConsoleWindow();

	wchar_t* currentDir = NULL;
	initCurrentDir(&currentDir);

	DirStack dirStack = { 0 };
	dirStackInit(&dirStack, currentDir);

	size_t dataCount = 0;
	FileArray fileArray = { 0 };
	DriveArray driveArray = { 0 };
	bool isFileArray = wcslen(currentDir) > 4;

	int highlight = 0;
	int downLast = 0;
	int upLast = 0;
	bool rightLast = false;
	bool leftLast = false;

	bool dirChanged = true;
	bool reprint = true;
	bool resort = true;
	wchar_t* retDir = NULL;

	bool nLast = false;
	bool tLast = false;
	bool sLast = false;
	bool cLast = false;
	bool wLast = false;
	bool pLast = false;

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
			reprint = true;

			highlight = 0;

			if (isFileArray)
				freeFileArray(&fileArray);
			else
				freeDriveArray(&driveArray);

			if (wcslen(currentDir) > 4)
			{
				fileArray = getFilesInDir(currentDir, loadSubdirs);
				dataCount = fileArray.count;
				isFileArray = true;
				
				// Needs to be sorted for highlight to be correct
				if (sortMethod == SORT_PATH || sortMethod == SORT_PATH_INV)
					sortMethod = SORT_NAME;
				sortFileArray(&fileArray, sortMethod);
				if (retDir)
				{
					for (int i = 0; i < fileArray.count; ++i)
					{
						if (wcscmp(fileArray.files[i].name, retDir) == 0)
						{
							highlight = (int)fileArray.count - 1 - i;
							break;
						}
					}

					free(retDir);
					retDir = NULL;
				}
			}
			else
			{
				driveArray = getDrives();
				dataCount = driveArray.count;
				isFileArray = false;

				// Needs to be sorted for highlight to be correct
				if (sortMethod == SORT_TYPE || sortMethod == SORT_TYPE_INV || 
					sortMethod == SORT_CREATE || sortMethod == SORT_CREATE_INV ||
					sortMethod == SORT_WRITE || sortMethod == SORT_WRITE_INV)
					sortMethod = SORT_NAME;
				sortDriveArray(&driveArray, sortMethod);
				if (retDir)
				{
					for (int i = 0; i < driveArray.count; ++i)
					{
						if (wcscmp(driveArray.drives[i].path, retDir) == 0)
						{
							highlight = (int)driveArray.count - 1 - i;
							break;
						}
					}

					free(retDir);
					retDir = NULL;
				}

			}

			resort = false;
			reprint = true;
		}
		if (resort)
		{
			resort = false;
			reprint = true;

			if (isFileArray)
				sortFileArray(&fileArray, sortMethod);
			else
				sortDriveArray(&driveArray, sortMethod);
		}
		if (reprint)
		{
			reprint = false;
			display(&fileArray, &driveArray, isFileArray, sortMethod, currentDir, highlight, consoleW, consoleH);
		}

		if (consoleWnd == GetForegroundWindow())
		{
			bool ctrl = GetKeyState(VK_CONTROL) & 0x8000;
			bool keydown = GetKeyState(VK_DOWN) & 0x8000;
			bool keyup = GetKeyState(VK_UP) & 0x8000;
			bool keyright = GetKeyState(VK_RIGHT) & 0x8000;
			bool keyleft = GetKeyState(VK_LEFT) & 0x8000;

			if (GetKeyState(VK_ESCAPE) & 0x8000)
				running = false;

			int keyRepWait = 6;
			if (keydown && (downLast + keyRepWait - 1) / keyRepWait != 1)
			{
				if (highlight + 1ull < dataCount)
				{
					if (ctrl)
						highlight = min((int)dataCount - 1, highlight + 5);
					else
						++highlight;
					reprint = true;
				}
			}
			if (keyup && (upLast + keyRepWait - 1) / keyRepWait != 1)
			{
				if (highlight > 0)
				{
					if (ctrl)
						highlight = max(0, highlight - 5);
					else
						--highlight;
					reprint = true;
				}
			}
			if (keyright && !rightLast)
			{
				loadSubdirs = !ctrl;
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
			if (keyleft && !leftLast)
			{
				loadSubdirs = !ctrl;
				dirChanged = true;

				bool error = false;
				currentDir = moveDirUp(currentDir, &error);
				if (error)
					dirChanged = false;

				retDir = dirStackPop(&dirStack);
			}

			if (keydown) ++downLast;
			else downLast = 0;
			if (keyup) ++upLast;
			else upLast = 0;
			rightLast = keyright;
			leftLast = keyleft;

			bool nKey = GetKeyState('N') & 0x8000;
			bool tKey = GetKeyState('T') & 0x8000;
			bool sKey = GetKeyState('S') & 0x8000;
			bool cKey = GetKeyState('C') & 0x8000;
			bool wKey = GetKeyState('W') & 0x8000;
			bool pKey = GetKeyState('P') & 0x8000;
			if (nKey && !nLast)
			{
				if (sortMethod == SORT_NAME)
					sortMethod = SORT_NAME_INV;
				else
					sortMethod = SORT_NAME;
				resort = true;
			}
			if (tKey && !tLast && isFileArray)
			{
				if (sortMethod == SORT_TYPE)
					sortMethod = SORT_TYPE_INV;
				else
					sortMethod = SORT_TYPE;
				resort = true;
			}
			if (sKey && !sLast)
			{
				if (sortMethod == SORT_SIZE)
					sortMethod = SORT_SIZE_INV;
				else
					sortMethod = SORT_SIZE;
				resort = true;
			}
			if (cKey && !cLast && isFileArray)
			{
				if (sortMethod == SORT_CREATE)
					sortMethod = SORT_CREATE_INV;
				else
					sortMethod = SORT_CREATE;
				resort = true;
			}
			if (wKey && !wLast && isFileArray)
			{
				if (sortMethod == SORT_WRITE)
					sortMethod = SORT_WRITE_INV;
				else
					sortMethod = SORT_WRITE;
				resort = true;
			}
			if (pKey && !pLast && !isFileArray)
			{
				if (sortMethod == SORT_PATH)
					sortMethod = SORT_PATH_INV;
				else
					sortMethod = SORT_PATH;
				resort = true;
			}

			nLast = nKey;
			tLast = tKey;
			sLast = sKey;
			cLast = cKey;
			wLast = wKey;
			pLast = pKey;
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