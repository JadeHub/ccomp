#include "validation_fixture.h"

class PtrValidationTest : public ValidationTest {};

TEST_F(PtrValidationTest, global)
{
	std::string code = R"(int* p;)";

	ExpectNoError(code);
}

TEST_F(PtrValidationTest, return_ptr)
{
	std::string code = R"(int g = 5; 
						int* fn()
						{
							return &g;
						})";

	ExpectNoError(code);
}


