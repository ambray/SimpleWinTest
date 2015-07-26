#include "test_defs.h"


static CHAR borders[] = {
	0x2d, // single dashed
	0x3d, // double
	0x5f, // underscore
};

static SLIST_HEADER gTestfuncHeader;
static SLIST_HEADER gFailedTestHeader;

static HANDLE gConsole;
static CONSOLE_SCREEN_BUFFER_INFO gConInfo;
static WORD gSavedAttribs = 0;

// Copies relevant data into a TEST_ENTRY structure, and appends it to the list of 
// tests to run.
static int createTestEntry(PSLIST_HEADER hdr, PCHAR namePtr,  void(WINAPI *fptr)(PTEST_CONTEXT))
{
	int status = ERROR_SUCCESS;
	PTEST_ENTRY tmp = NULL;
	DWORD tmpLen = 0;


	if (NULL == hdr || NULL == namePtr || NULL == fptr)
		return ERROR_INVALID_PARAMETER;

	__try {

		if (NULL == (tmp = (PTEST_ENTRY)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*tmp)))) {
			status = ERROR_OUTOFMEMORY;
			__leave;
		}

		tmp->loc = fptr;
		tmp->testName = namePtr;

		InterlockedPushEntrySList(hdr, &tmp->list);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		status = GetExceptionCode();
	}

	return status;
}

static PCHAR __forceinline unmangle(PCHAR exportName, PCHAR prefix, BOOL isMutable)
{
	DWORD prefixLen = 0;
	DWORD exportLen = 0;
	PCHAR current = NULL;
	PCHAR offset = NULL;

	if (NULL == exportName || NULL == prefix) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return NULL;
	}

	prefixLen = strlen(prefix);
	exportLen = strlen(exportName);

	if (exportLen <= prefixLen) {
		SetLastError(ERROR_INVALID_PARAMETER);
		return NULL;
	}

	current = (PCHAR)(exportName + prefixLen);

	if (isMutable && NULL != (offset = strstr(exportName, "@4"))) {
		*offset = '\0';
	}

	return current;
}

// Prints a border covering the width of the console (or MAX_PATH
// if the console width exceeds the stack-allocated buffer size).
static void __forceinline printSpacer(CHAR border)
{
	CHAR buffer[MAX_PATH + 1] = { 0 };
	DWORD width = 0;

	width = gConInfo.dwSize.X;

	if (width == 0 || width > MAX_PATH) {
		width = MAX_PATH;
		buffer[width] = '\n';
	}

	memset(buffer, border, width);

	if (INVALID_HANDLE_VALUE != gConsole)
		SetConsoleTextAttribute(gConsole, FOREGROUND_GREEN | FOREGROUND_INTENSITY);

	printf("%s", buffer);
}

// Prints a formatted console message with text colored according to 'color',
// and will wrap it in a border combination specified by the SpacerCode provided.
static void printColor(PCHAR text, WORD color, SpacerCodes spacer)
{
	CHAR code = 0;
	CHAR buffer[MAX_PATH + 1] = { 0 };
	DWORD len = 0;

	_snprintf(buffer, MAX_PATH, "| %s", text);

	if (spacer == SpacerCodeSingleBottom || spacer == SpacerCodeSingleTop || spacer == SpacerCodeSingleEncaps) {
		code = borders[0];
	}
	else if (spacer == SpacerCodeDoubleBottom || spacer == SpacerCodeDoubleTop || spacer == SpacerCodeDoubleEncaps)
		code = borders[1];
	else {
		code = borders[2];
	}

	if (!(spacer == SpacerCodeNoBorder) && (spacer & 1))
		printSpacer(code);

	if (INVALID_HANDLE_VALUE != gConsole)
		SetConsoleTextAttribute(gConsole, color | FOREGROUND_INTENSITY);

	printf("%s\n", buffer);

	if (!(SpacerCodeNoBorder == spacer) && (SpacerCodeDoubleEncaps == spacer || SpacerCodeSingleEncaps == spacer || !(spacer & 1)))
		printSpacer(code);

	if (INVALID_HANDLE_VALUE != gConsole && gSavedAttribs)
		SetConsoleTextAttribute(gConsole, gSavedAttribs);

}

static int printBegin(PCHAR exportName)
{
	int status = ERROR_SUCCESS;
	CHAR buffer[MAX_PATH + 1] = { 0 };
	PCHAR offset = NULL;
	PCHAR current = NULL;

	if (NULL == exportName)
		return ERROR_INVALID_PARAMETER;

	if (NULL == (current = unmangle(exportName, TEST_STRING, FALSE))) {
		return GetLastError();
	}

	_snprintf(buffer, MAX_PATH, "[*] Preparing to run %s...", current);
#ifndef _WIN64
	if (NULL != (offset = strstr(buffer, "@4"))) {
		*offset = '\0'; // remove stdcall name decoration
	}
#endif
		
	printColor(buffer, FOREGROUND_GREEN, SpacerCodeSingleTop);

	return status;
}

// Set up to print the result of our operations.
static int printStatus(PTEST_CONTEXT ctx, DWORD stat)
{
	int status = ERROR_SUCCESS;
	CHAR buffer[MAX_PATH + 1] = { 0 };
	PCHAR string = NULL;
	PCHAR prefix = NULL;

	prefix = (AssertSuccess == ctx->code) ? "[*]" : "[x]";

	switch (ctx->code) {
	case AssertSuccess:
		string = "The task completed successfully.";
		break;
	case AssertFailApiError:
		string = "An API Failure has occurred around line: ";
		break;
	case AssertFailEq:
		string = "Items were not expected to be equal! Test failed around line: ";
		break;
	case AssertFailNotEq:
		string = "Items were expected to be equal! Test failed around line: ";
		break;
	case AssertFailException:
		string = "An exception occurred! Code: ";
		break;
	case AssertFailFlagNotRestored:
		string = "A flag was modified and not restored! Error occurred around line: ";
		break;
	case AssertFailGeneric:
		string = "A generic failure was observed around line: ";
		break;
	case AssertFailNotGt:
		string = "Item was expected to be greater! Test failed around line: ";
		break;
	case AssertFailNotGtEq:
		string = "Item was expected to be greater or equal! Test failed around line: ";
		break;
	case AssertFailNotLt:
		string = "Item was expected to be less than! Test failed around line: ";
		break;
	case AssertFailNotLtEq:
		string = "Item was expected to be less than or equal! Test failed around line: ";
		break;
	case AssertFailRegClobbered:
		string = "Register that was expected to be restored was clobbered! Test failed around line: ";
		break;
	case AssertFailStackBroken:
		string = "Something has broken the stack! Check for unbounded copies...";
		break;
	case AssertFailStackUnbalanced:
		string = "The stack is unbalanced! Either the stack pointer or base pointer was not properly restored! Test failed around line: ";
		break;
	default:
		string = "Something went horribly wrong!";
	};

	_snprintf(buffer, MAX_PATH, "%s %s %lu", prefix, string, ctx->line);

	printColor(buffer, (ctx->code == AssertSuccess) ? FOREGROUND_GREEN : FOREGROUND_RED, SpacerCodeSingleBottom);

	return status;
}

// Looks for exported tests to be run, locates relevant information and
// passes it to be added to list of tests.
static int walkExportTable(HMODULE mod, PIMAGE_EXPORT_DIRECTORY pexp, PCHAR prefix)
{
	int status = ERROR_SUCCESS;
	
	PDWORD namesAddress = NULL;
	PWORD ordAddress = NULL;
	PDWORD codeAddress = NULL;

	DWORD iter;
	WORD currentOrd;
	PCHAR currentName = NULL;
	PCHAR tmp = NULL;
	void(WINAPI *tf)(PTEST_CONTEXT) = NULL;
	DWORD currentLen = 0;

	if (NULL == pexp || NULL == mod || NULL == prefix)
		return ERROR_INVALID_PARAMETER;

	namesAddress = (PDWORD)((PBYTE)mod + pexp->AddressOfNames);
	ordAddress = (PWORD)((PBYTE)mod + pexp->AddressOfNameOrdinals);
	codeAddress = (PDWORD)((PBYTE)mod + pexp->AddressOfFunctions);

	for (iter = 0; iter < pexp->NumberOfNames; ++iter) {

		//printf("Exported method: %s @ ordinal: %d\n", (PCHAR)((PBYTE)mod + namesAddress[iter]), (WORD)((PBYTE)mod + ordAddress[iter]));
		currentName = (PCHAR)((PBYTE)mod + namesAddress[iter]);
		if (0 == strncmp(prefix, currentName, strlen(prefix))) {
			currentOrd = ordAddress[iter];
			if (NULL == (tmp = (PCHAR)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, strlen(currentName) + 1))) {
				printf("Failed to allocate memory!\n");
				status = ERROR_OUTOFMEMORY;
				break;
			}

			tf = (void(WINAPI*)(PTEST_CONTEXT))((PBYTE)mod + codeAddress[currentOrd]);

			strncpy(tmp, currentName, strlen(currentName));
			if (ERROR_SUCCESS != (status = createTestEntry(&gTestfuncHeader, tmp, tf)))
				break;
		}
	}

#ifdef _WIN64
	if (ERROR_SUCCESS == status && NULL == gTestfuncHeader.HeaderX64.NextEntry)
#else
	if (ERROR_SUCCESS == status && NULL == gTestfuncHeader.Next.Next)
#endif
		status = ERROR_NO_MORE_MATCHES;

	return status;
}

// Attempt to find the export directory. This could (potentially) fail and/or
// produce unexpected results if both no tests are defined, and no functions are
// exported.
static int findExports(HMODULE mod, PIMAGE_EXPORT_DIRECTORY* exp)
{
	int status = ERROR_SUCCESS;
	PIMAGE_NT_HEADERS		 nthdr;
	PIMAGE_DATA_DIRECTORY	 exportLoc;
	PIMAGE_EXPORT_DIRECTORY  exports;

	__try {
		if (NULL == mod || NULL == exp) {
			status = ERROR_INVALID_PARAMETER;
			__leave;
		}

		nthdr = (PIMAGE_NT_HEADERS)((PBYTE)mod + ((PIMAGE_DOS_HEADER)mod)->e_lfanew);
		exportLoc = (PIMAGE_DATA_DIRECTORY)&nthdr->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
		exports = (PIMAGE_EXPORT_DIRECTORY)((PBYTE)mod + exportLoc->VirtualAddress);

		*exp = exports;

	} 
	__except (EXCEPTION_EXECUTE_HANDLER) {
		printf("An exception occurred!\n");
		status = GetExceptionCode();
	}

	return status;
}


static DWORD testRunner(PTEST_ENTRY test)
{
	DWORD status = ERROR_SUCCESS;
	TEST_CONTEXT ctx = { 0 };

	DWORD cookie = COOKIE;
	__try {
		if (ERROR_SUCCESS != (status = printBegin(test->testName))) {
			__leave;
		}

		test->loc(&ctx);

	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		ctx.code = AssertFailException;
		ctx.line = GetExceptionCode();
	}

	if (AssertSuccess == ctx.code && cookie != COOKIE) {
		ctx.code = AssertFailStackBroken;
	}

	printStatus(&ctx, status);

	test->testStatus = ctx.code;

	return status;
}


// Prints the test results, giving a brief synopsys of what did and didn't pass.
static void printFailed(PSLIST_HEADER hdr, DWORD failCount, DWORD testCount)
{
	CHAR buffer[MAX_PATH + 1] = { 0 };
	PTEST_ENTRY tmp = NULL;
	PCHAR offset = NULL;

	if (NULL == hdr || failCount > testCount)
		return;

	_snprintf(buffer, MAX_PATH, "Tests Complete. Status: %lu out of %lu succeeded.", testCount - failCount, testCount);

	printColor(buffer, FOREGROUND_GREEN, SpacerCodeDoubleEncaps);

	printSpacer(borders[0]);

	if (failCount > 0) {
		while (NULL != (tmp = InterlockedPopEntrySList(hdr))) {
			ZeroMemory(buffer, MAX_PATH);
			_snprintf(buffer, MAX_PATH, "Test %s failed with code %lu", unmangle(tmp->testName, TEST_STRING, TRUE), tmp->testStatus);

			printColor(buffer, FOREGROUND_RED, SpacerCodeNoBorder);

			HeapFree(GetProcessHeap(), 0, tmp->testName);
			HeapFree(GetProcessHeap(), 0, tmp);
		}
	}

	printSpacer(borders[0]);

}

// This runs each test in sequence. In the future, it will likely
// provide options to debug the running test thread (to keep tabs on things like
// stack unbalance, registers not being properly preserved across function calls, etc.)
static int run(PSLIST_HEADER hdr)
{
	int status = ERROR_SUCCESS;
	CHAR buffer[MAX_PATH + 1] = { 0 };
	HANDLE hThread = NULL;
	BOOL first = TRUE;
	PTEST_ENTRY testEntry = NULL;
	DWORD testCount = 0;
	DWORD failCount = 0;
	PTEST_ENTRY tmp = NULL;

	while (NULL != (testEntry = (PTEST_ENTRY)InterlockedPopEntrySList(hdr))) {

		if (first) {
			testCount = 1;
			tmp = testEntry->list.Next;
			while (NULL != tmp) {
				tmp = tmp->list.Next;
				++testCount;
			}

			_snprintf(buffer, MAX_PATH, "%s %lu %s", "Preparing to run ", testCount, "tests.");
			printColor(buffer, FOREGROUND_GREEN, SpacerCodeDoubleEncaps);

			first = FALSE;
		}

		if (NULL == (hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)testRunner, testEntry, 0, NULL)))
			continue;

		WaitForSingleObject(hThread, INFINITE);
		CloseHandle(hThread);

		if (AssertSuccess != testEntry->testStatus) {
			InterlockedIncrement(&failCount);
			InterlockedPushEntrySList(&gFailedTestHeader, &testEntry->list);
		}
		else {
			// cleanup
			HeapFree(GetProcessHeap(), 0, testEntry->testName);
			HeapFree(GetProcessHeap(), 0, testEntry);
		}
	}

	printFailed(&gFailedTestHeader, failCount, testCount);

	return status;
}

// Given the module to check, find all defined tests.
static int discover(HMODULE mod)
{
	int status = ERROR_SUCCESS;
	PIMAGE_EXPORT_DIRECTORY pexp = NULL;
	char* name;
	DWORD nameOffset;

	DWORD ind = 0;

	if (ERROR_SUCCESS != (status = findExports(mod, &pexp))) {
		return status;
	}

	if (ERROR_SUCCESS != (status = walkExportTable(mod, pexp, TEST_STRING))) {
		return status;
	}

	return status;
}

static __forceinline HMODULE getHmod(HMODULE inParam)
{
	return (NULL == inParam) ? GetModuleHandle(NULL) : inParam;
}

int runTests(HMODULE mod)
{
	HMODULE testOn;
	PTEST_ENTRY tmp = NULL;
	int status = ERROR_SUCCESS;

	InitializeSListHead(&gTestfuncHeader);
	InitializeSListHead(&gFailedTestHeader);
	testOn = getHmod(mod);

	__try {

		if (INVALID_HANDLE_VALUE == (gConsole = GetStdHandle(STD_OUTPUT_HANDLE))) {
			status = GetLastError();
			__leave;
		}

		if (!GetConsoleScreenBufferInfo(gConsole, &gConInfo)) {
			status = GetLastError();
			__leave;
		}

		gSavedAttribs = gConInfo.wAttributes;

		if (ERROR_SUCCESS != (status = discover(testOn))) {
			__leave;
		}

		if (ERROR_SUCCESS != (status = run(&gTestfuncHeader))) {
			__leave;
		}
	}
	__finally {
		while (NULL != (tmp = (PTEST_ENTRY)InterlockedPopEntrySList(&gTestfuncHeader))) {
			HeapFree(GetProcessHeap(), 0, tmp->testName);
			HeapFree(GetProcessHeap(), 0, tmp);
		}

		if (gSavedAttribs && INVALID_HANDLE_VALUE != gConsole) {
			SetConsoleTextAttribute(gConsole, gSavedAttribs);
		}
	}

	return status;
}

