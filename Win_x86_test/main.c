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

}

int main(int argc, char** argv, char** envp)
{
	return runTests(NULL);
}
