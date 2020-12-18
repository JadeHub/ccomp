#include "validation_fixture.h"

class ForSmntValidationTest : public ValidationTest {};

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

	ExpectNoError(code);
}
