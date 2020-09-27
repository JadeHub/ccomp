#include "validation_fixture.h"

class BinOpValidationTest : public ValidationTest {};

TEST_F(BinOpValidationTest, greater_than_lhs_not_int)
{
	std::string code = R"(
	int foo()
	{
		struct A {int i;} a;
		
		return a < 5;
	}
	)";

	ExpectError(code, ERR_INCOMPATIBLE_TYPE);
}
