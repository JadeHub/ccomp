#include "validation_fixture.h"

class PtrValidationTest : public ValidationTest {};

TEST_F(PtrValidationTest, foo)
{
	std::string code = R"(
	int main()
	{
		int i = 5;
		int* ptr = &i;
		return *ptr;
	}
	)";

	ExpectNoError(code);	
}


void fn1(), *fn2(int);

void fn1() {}
void* fn2(int) { return 0; }

void x()
{
	int******** i;

	fn1();
	fn2(5);
}