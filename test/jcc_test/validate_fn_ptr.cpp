#include "validation_fixture.h"

class FnPtrSmntValidationTest : public ValidationTest {};

void foo() {}
void foo1() {}
void (*fn)() = foo;


TEST_F(FnPtrSmntValidationTest, foo)
{
	(*fn)();
	fn();
	fn = foo1;
	fn = &foo;

	std::string code = R"(
	void foo() {}

	void test()
	{
		void (*fn)() = foo;
	}
	
	)";

	ExpectNoError(code);
}