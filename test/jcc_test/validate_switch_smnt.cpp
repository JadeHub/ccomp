#include "validation_fixture.h"

class SwitchValidationTest : public ValidationTest {};

TEST_F(SwitchValidationTest, valid)
{
	std::string code = R"(
	void foo()
	{
		int i;
		switch (i)
		{
			default: 
				return;
			case 1:
				return;
			case 2:
				return;
		}
	}
	)";

	ExpectNoError(code);
}

TEST_F(SwitchValidationTest, err_missing_colon)
{
	std::string code = R"(
	void foo()
	{
		int i;
		switch (i)
		{
			default: 
				return;
			case 1
				return;
			case 2:
				return;
		}
	}
	)";

	ExpectError(code, ERR_SYNTAX);
}

TEST_F(SwitchValidationTest, err_no_cases)
{
	std::string code = R"(
	void foo()
	{
		int i;
		switch (i);
	}
	)";

	ExpectError(code, ERR_INVALID_SWITCH);
}

TEST_F(SwitchValidationTest, err_multiple_default)
{
	std::string code = R"(
	void foo()
	{
		int i;
		switch (i)
		{
			default: 
				return;
			default:
				return;
		}
	}
	)";

	ExpectError(code, ERR_SYNTAX);
}

TEST_F(SwitchValidationTest, err_missing_case_expr)
{
	std::string code = R"(
	void foo()
	{
		int i;
		switch (i)
		{
			case:
				return;
		}
	}
	)";

	ExpectError(code, ERR_SYNTAX);
}

TEST_F(SwitchValidationTest, err_no_smnt)
{
	std::string code = R"(
	void foo()
	{
		int i;
		switch (i)
		{
		case 1:
		case 2:
		}
	}
	)";

	ExpectError(code, ERR_INVALID_SWITCH);
}

TEST_F(SwitchValidationTest, err_default_with_expr)
{
	std::string code = R"(
	void foo()
	{
		int i;
		switch (i)
		{
		case 1:
		default 2:
		}
	}
	)";

	ExpectError(code, ERR_SYNTAX);
}


TEST_F(SwitchValidationTest, valid_missing_case_statement)
{
	std::string code = R"(
	void foo()
	{
		int i;
		switch (i)
		{
			case 1:
			case 2:
				break;
		}
	}
	)";

	ExpectNoError(code);
}