#include "validation_fixture.h"

class ForSmntValidationTest : public ValidationTest {};

TEST_F(ForSmntValidationTest, foo)
{
	std::string code = R"(
	void foo()
	{
		int i = 0;
	}
	)";

	ExpectNoError(code);
}
