#include "validation_fixture.h"

class GotoValidationTest : public CompilerTest {};

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

	ExpectCompilerError(code, ERR_DUP_LABEL);
}

TEST_F(GotoValidationTest, err_unknown_label)
{
	std::string code = R"(
	void foo()
	{
		goto unknown;
	}
	)";

	ExpectCompilerError(code, ERR_UNKNOWN_LABEL);
}

TEST_F(GotoValidationTest, goto_before_label)
{
	std::string code = R"(
	void foo()
	{
		goto l1;
l1: ;
	}
	)";

	ExpectNoError(code);
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