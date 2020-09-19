#include "validation_fixture.h"

class StringLiteralValidationTest : public ValidationTest {};

TEST_F(StringLiteralValidationTest, local_str_literal)
{
	std::string code = R"(
	int main()
	{
		char* c = "Hello";
		return *c;
	}	
	)";

	ExpectNoError(code);
}
