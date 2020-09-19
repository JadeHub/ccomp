#include "validation_fixture.h"

class ArrayValidationTest : public ValidationTest {};

TEST_F(ArrayValidationTest, var_array_decl)
{
	std::string code = R"(
	void foo()
	{
		int j[5];
	}
	)";

	ExpectNoError(code);
}

TEST_F(ArrayValidationTest, var_array_param)
{
	std::string code = R"(
	void foo(int j[5])
	{
	}
	)";

	ExpectNoError(code);
}

TEST_F(ArrayValidationTest, var_array_param_decl_no_name)
{
	std::string code = R"(
	void foo(int [5]);
	)";

	ExpectNoError(code);
}

TEST_F(ArrayValidationTest, array_subscript)
{
	std::string code = R"(
	char foo()
	{
		char* str = "Hello";
		return str[2];
	}
	)";

	ExpectNoError(code);
}

TEST_F(ArrayValidationTest, err_array_subscript_not_ptr)
{
	std::string code = R"(
	char foo()
	{
		int i = 5;
		return i[2];
	}
	)";

	ExpectError(code, ERR_INCOMPATIBLE_TYPE);
}


TEST_F(ArrayValidationTest, err_array_subscript_not_ptr_idx)
{
	std::string code = R"(
	char foo()
	{
		char* str = "test";
		struct A {int i;} a;
		return str[a];
	}
	)";

	ExpectError(code, ERR_INCOMPATIBLE_TYPE);
}

TEST_F(ArrayValidationTest, ptr_subscript)
{
	std::string code = R"(
	char* fn() {}
	int idx() {return 2;}
	char foo()
	{
		return fn()[idx()];
	}
	)";

	ExpectNoError(code);
}