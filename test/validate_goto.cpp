#include "validation_fixture.h"

class GotoValidationTest : public ValidationTest {};

TEST_F(GotoValidationTest, multi_label_goto)
{
	std::string code = R"(
	void foo()
	{
l1: l2:
		goto l1;
	}
	)";

	ExpectNoError(code);
}

TEST_F(GotoValidationTest, err_dup_label)
{
	std::string code = R"(
	void foo()
	{
l1:
l1:	;
	}
	)";

	ExpectError(code, ERR_DUP_LABEL);
}

TEST_F(GotoValidationTest, dup_label_different_fn)
{
	std::string code = R"(
	void foo()
	{
l1:;
	}
	
	void bar()
	{
l1:	;
	}
	)";

	ExpectNoError(code);
}