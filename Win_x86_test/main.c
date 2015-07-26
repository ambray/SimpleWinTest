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

int main(int argc, char** argv, char** envp)
{
	return runTests(NULL);
}
