#include "validation_fixture.h"

class SwitchValidationTest : public CompilerTest {};

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

TEST_F(SwitchValidationTest, multiline_case)
{
	std::string code = R"(

	void some_fn();

	void foo()
	{
		int i;
		switch (i)
		{
			case 1:
				some_fn();
				break;
			case 2:
			{
				some_fn();
				break;
			}
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

	ExpectCompilerError(code, ERR_SYNTAX);
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

	ExpectCompilerError(code, ERR_INVALID_SWITCH);
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

	ExpectCompilerError(code, ERR_INVALID_SWITCH);
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

	ExpectCompilerError(code, ERR_SYNTAX);
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

	ExpectCompilerError(code, ERR_INVALID_SWITCH);
}

TEST_F(SwitchValidationTest, err_spurious_case)
{
	std::string code = R"(
	void foo()
	{
		case 1: x;
	}
	)";

	ExpectCompilerError(code, ERR_SYNTAX);
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

	ExpectCompilerError(code, ERR_SYNTAX);
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

TEST_F(SwitchValidationTest, switch_on_enum)
{
	std::string code = R"(

	typedef enum
	{
		A_1,
		A_2
	}A;

	typedef enum
	{
		B_1,
		B_2
	}B;

	int foo()
	{
		B b = B_1;
		switch (b)
		{
			case B_1:
				return A_1;
			case B_2:
				return A_2;
		}
		return -1;
	}
	)";

	ExpectNoError(code);
}