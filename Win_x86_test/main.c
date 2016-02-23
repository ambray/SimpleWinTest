#include "test_defs.h"


TEST(Initial, Herp)
{
	ASSERT_EQUAL(1, 1);
	printf("Herp\n");
	ASSERT_NEQUAL(1, 1);
}

TEST(Initial, Derp)
{
	PBYTE test = NULL;

	printf("Derp\n");
	FAIL_WITH_CODE(AssertFailFlagNotRestored);

}

TEST(Initial, AnotherTest)
{
	DWORD a = 10;
	DWORD b = 9;

	ASSERT_GT(a, b);
}

TEST(Initial, YetAnotherTest)
{
	DWORD a = 1;
	DWORD b = 1;

	ASSERT_OR_SKIP(a, b, 1);
}

TEST(Initial, SkipTestTwo)
{
	SKIP_IF(1 == 1);
}

int main(int argc, char** argv, char** envp)
{
	return runTests(NULL);
}
