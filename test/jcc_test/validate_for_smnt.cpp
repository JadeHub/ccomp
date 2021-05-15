#include "validation_fixture.h"

class ForSmntValidationTest : public CompilerTest {};

TEST_F(ForSmntValidationTest, foo)
{
	std::string code = R"(
	void foo()
	{
		for(int i = 0; i< 10; i++)
		{
			s();
		}		
	}
	)";

//	ExpectNoError(code);
}
