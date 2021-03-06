#include "validation_fixture.h"

class FnCallValidationTest : public CompilerTest {};

TEST_F(FnCallValidationTest, err_var_shaddows_fn)
{
	std::string code = R"(
	void foo()
	{
	}

	void bar()
	{
		int foo = 0;
		foo();
	}
	)";

	ExpectCompilerError(code, ERR_INCOMPATIBLE_TYPE);
}

TEST_F(FnCallValidationTest, err_undefined)
{
	std::string code = R"(
	void foo()
	{
		bar();
	}
	)";

	ExpectCompilerError(code, ERR_UNKNOWN_IDENTIFIER);
}

TEST_F(FnCallValidationTest, err_too_many_params)
{
	std::string code = R"(
	void bar();
	void foo()
	{
		bar(2);
	}
	)";

	ExpectCompilerError(code, ERR_INVALID_PARAMS);
}

TEST_F(FnCallValidationTest, err_too_many_params2)
{
	std::string code = R"(
	void bar(int);
	void foo()
	{
		bar(2, 3);
	}
	)";

	ExpectCompilerError(code, ERR_INVALID_PARAMS);
}

TEST_F(FnCallValidationTest, err_incorrect_param_type)
{
	std::string code = R"(
	void bar(struct A{int a;} aa);
	void foo()
	{
		bar(5);
	}
	)";

	ExpectCompilerError(code, ERR_INCOMPATIBLE_TYPE);
}

TEST_F(FnCallValidationTest, err_incorrect_param_type2)
{
	std::string code = R"(
	void bar(struct A{int a;} aa);
	void foo()
	{
		int i = 5;
		bar(i);
	}
	)";

	ExpectCompilerError(code, ERR_INCOMPATIBLE_TYPE);
}

TEST_F(FnCallValidationTest, param_fn_call)
{
	std::string code = R"(
	int bar1();
	void bar2(int);
	void foo()
	{
		bar2(bar1());
	}
	)";

	ExpectNoError(code);
}

TEST_F(FnCallValidationTest, err_param_fn_call)
{
	std::string code = R"(
	void bar1();
	void bar2(int);
	void foo()
	{
		bar2(bar1());
	}
	)";

	ExpectCompilerError(code, ERR_INCOMPATIBLE_TYPE);
}

TEST_F(FnCallValidationTest, err_too_few_params)
{
	std::string code = R"(
	void bar(int);
	void foo()
	{
		bar();
	}
	)";

	ExpectCompilerError(code, ERR_INVALID_PARAMS);
}

TEST_F(FnCallValidationTest, err_too_few_params2)
{
	std::string code = R"(
	void bar(int, int);
	void foo()
	{
		bar(2);
	}
	)";

	ExpectCompilerError(code, ERR_INVALID_PARAMS);
}

TEST_F(FnCallValidationTest, no_param)
{
	std::string code = R"(
	void bar();
	void foo()
	{
		bar();
	}
	)";

	ExpectNoError(code);
}

TEST_F(FnCallValidationTest, int_param)
{
	std::string code = R"(
	void bar(int);
	void foo()
	{
		bar(5);
	}
	)";

	ExpectNoError(code);
}

TEST_F(FnCallValidationTest, char_short_param)
{
	std::string code = R"(
	int foo(char c, short s)
	{
		return  c + s;
	}

	int main()
	{
		char c = -12;
		short s = -2;

		return foo(c, s);
	}
	)";

	ExpectNoError(code);
}

