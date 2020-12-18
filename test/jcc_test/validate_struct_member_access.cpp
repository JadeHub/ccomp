#include "validation_fixture.h"

class StructMemberAccessValidationTest : public ValidationTest {};

/*TEST_F(StructMemberAccessValidationTest, err_missing_member)
{
	std::string code = R"(

	void foo()
	{
		struct A {int a; int b} a;

		a. = 1;	
	}		
	)";

	ExpectSyntaxErrors(code);
	
}
*/