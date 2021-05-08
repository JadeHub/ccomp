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

TEST_F(StringLiteralValidationTest, local_str_literal_line_cont)
{
	std::string code = R"(
	int main()
	{
		char* c = "He\
llo";
		return *c;
	}	
	)";

	ExpectNoError(code);
}

TEST_F(StringLiteralValidationTest, global_str_literal_line_cont)
{
	std::string code = R"(
	char* c = "He\
llo";

	int main()
	{		
		return *c;
	}	
	)";

	ExpectNoError(code);
}
