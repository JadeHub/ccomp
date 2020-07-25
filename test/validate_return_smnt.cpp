#include "validation_fixture.h"

class ReturnValidationTest : public ValidationTest {};


TEST_F(ReturnValidationTest, err_return_when_void)
{
	std::string code = R"(
	void foo()
	{
		return 5;
	}
	)";

	ExpectError(code, ERR_INVALID_RETURN);
}

TEST_F(ReturnValidationTest, err_missing_expr)
{
	std::string code = R"(
	int foo()
	{
		return;
	}
	)";

	ExpectError(code, ERR_INVALID_RETURN);
}

TEST_F(ReturnValidationTest, err_wrong_type)
{
	std::string code = R"(
	struct A {int a; } foo()
	{
		return 5;
	}
	)";

	ExpectError(code, ERR_INVALID_RETURN);
}

TEST_F(ReturnValidationTest, return_struct)
{
	std::string code = R"(
	struct A 
	{
		int a; 
	};

	struct A foo()
	{
		struct A anA;
		return anA;
	}
	)";

	ExpectNoError(code);
}

TEST_F(ReturnValidationTest, return_bin_expr)
{
	std::string code = R"(
	int foo()
	{
		struct A {int a; int b;} a;
        a.a = 5;
        a.b = 1;

        return a.b + a.a;
	}
	)";

	ExpectNoError(code);
}

TEST_F(ReturnValidationTest, return_member_access)
{
	std::string code = R"(
	int foo()
	{
		struct A
		{
			int a;
			int b;
		} a;
        a.a = 5;
        a.b = 1;

        return a.b + a.a;
	}
	)";

	ExpectNoError(code);
}