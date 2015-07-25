#pragma once

#include <Windows.h>
#include <stdio.h>
#include <ImageHlp.h>

#define COOKIE	0xc0ffee

typedef enum {
	SpacerCodeSingleTop = 1,
	SpacerCodeSingleBottom,
	SpacerCodeDoubleTop = 3,
	SpacerCodeDoubleBottom,
	SpacerCodeSingleEncaps = 5,
	SpacerCodeDoubleEncaps = 7,
} SpacerCodes;

typedef enum {
	AssertSuccess,
	AssertFailNotEq,
	AssertFailNotLt,
	AssertFailNotGt,
	AssertFailNotLtEq,
	AssertFailNotGtEq,
	AssertFailEq,
	AssertFailStackUnbalanced,
	AssertFailRegClobbered,
	AssertFailFlagNotRestored,
	AssertFailGeneric,
	AssertFailApiError,
	AssertFailStackBroken,
	AssertFailException,
} AssertCode;

AssertCode __forceinline validateContext(PCONTEXT oldCtx, PCONTEXT newCtx)
{
	AssertCode code = AssertSuccess;

	if (NULL == oldCtx || NULL == newCtx)
		return AssertFailApiError;

#ifdef _WIN64
	if (oldCtx->Rsp != newCtx->Rsp || oldCtx->Rbp != newCtx->Rbp) {
		code = AssertFailStackUnbalanced;
	}
	else if ((oldCtx->Rbx != newCtx->Rbx) || (oldCtx->Rsi != newCtx->Rsi) || (oldCtx->Rdi != newCtx->Rdi)) {
		code = AssertFailRegClobbered;
	}
	else if ((oldCtx->R12 != newCtx->R12) || (oldCtx->R13 != newCtx->R13) || (oldCtx->R14 != newCtx->R14) || (oldCtx->R15 != newCtx->R15)) {
		code = AssertFailRegClobbered;
	}
#else
	if (oldCtx->Esp != newCtx->Esp || oldCtx->Ebp != newCtx->Ebp) {
		code = AssertFailStackUnbalanced;
	}
	else if ((oldCtx->Ebx != newCtx->Ebx) || (oldCtx->Esi != newCtx->Esi) || (oldCtx->Edi != newCtx->Edi)) {
		code = AssertFailRegClobbered;
	}
#endif

	return code;
}


typedef struct {
	DWORD		line;
	AssertCode	code;
	SIZE_T		storedEsp;
	SIZE_T		storedEbp;
	CONTEXT		octx;
	CONTEXT		nctx;
} TEST_CONTEXT, *PTEST_CONTEXT;

typedef struct {
	SLIST_ENTRY	list;
	PCHAR		testName;
	void(WINAPI *loc)(PTEST_CONTEXT);
} TEST_ENTRY, *PTEST_ENTRY;



// This will essentially only modify how our test methods get name-mangled to make them unique
// so they can be found when runTests gets called
#ifdef _WIN64
#define TEST_STRING			"test_set_0af2c_"
#else
#define TEST_STRING			"_test_set_0af2c_"
#endif
#define TEST_PREFIX(x, ...)	test_set_0af2c_ ## x ## _ ## __VA_ARGS__

#ifdef __cplusplus
#define TEST(set, func)	extern "C" void __declspec(dllexport) WINAPI TEST_PREFIX(set, func)(PVOID ctx)
#else
#define TEST(set, func)	void __declspec(dllexport) WINAPI TEST_PREFIX(set, func)(PTEST_CONTEXT TEST_CTX_PTR)
#endif

int runTests(HMODULE mod);

#define ASSERT_EQUAL(x, y)\
	if(x != y) { TEST_CTX_PTR->line = __LINE__; TEST_CTX_PTR->code = AssertFailNotEq; return; }

#define ASSERT_NEQUAL(x, y)\
	if(x == y) { TEST_CTX_PTR->line = __LINE__; TEST_CTX_PTR->code = AssertFailEq; return; }

#define ASSERT_LT(x, y)\
	if(x >= y) { TEST_CTX_PTR->line = __LINE__; TEST_CTX_PTR->code = AssertFailNotLt; return; }

#define ASSERT_GT(x, y)\
	if(x <= y) { TEST_CTX_PTR->line = __LINE__; TEST_CTX_PTR->code = AssertFailNotGt; return; }

#define ASSERT_LTE(x, y)\
	if(!(x <= y)) { TEST_CTX_PTR->line = __LINE__; TEST_CTX_PTR->code = AssertFailNotLtEq; return; }

#define ASSERT_GTE(x, y)\
	if(!(x >= y)) { TEST_CTX_PTR->line = __LINE__; TEST_CTX_PTR->code = AssertFailNotGtEq; return; }

#define FAIL()\
	{ TEST_CTX_PTR->line = __LINE__; TEST_CTX_PTR->code = AssertFailGeneric; return; }

#define FAIL_WITH_CODE(code)\
	{ TEST_CTX_PTR->line = __LINE__; TEST_CTX_PTR->code = code; return; }