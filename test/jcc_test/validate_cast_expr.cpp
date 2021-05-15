#include "validation_fixture.h"

class CastValidationTest : public CompilerTest {};

TEST_F(CastValidationTest, int_cast)
{
	std::string code = R"(
	
	char foo()
	{
		int j = 5;
		
		return (char)j;
	}
	)";

	ExpectNoError(code);
}
