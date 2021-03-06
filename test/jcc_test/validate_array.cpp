#include "validation_fixture.h"

class ArrayValidationTest : public CompilerTest {};

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

TEST_F(ArrayValidationTest, ptr_subscript)
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

TEST_F(ArrayValidationTest, array_subscript)
{
	std::string code = R"(
	int foo()
	{
		int i[2];
		i[1] = 5;
		return i[1];
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

	ExpectCompilerError(code, ERR_INCOMPATIBLE_TYPE);
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

	ExpectCompilerError(code, ERR_INCOMPATIBLE_TYPE);
}

TEST_F(ArrayValidationTest, ptr_fn_subscript)
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

TEST_F(ArrayValidationTest, err_array_init_too_few)
{
	std::string code = R"(
	void foo()
	{
		int i[2] = {1};
	}
	)";

	ExpectCompilerError(code, ERR_INVALID_INIT);
}

TEST_F(ArrayValidationTest, err_array_init_too_many)
{
	std::string code = R"(
	void foo()
	{
		int i[2] = {1, 2, 3};
	}
	)";

	ExpectCompilerError(code, ERR_INVALID_INIT);
}

TEST_F(ArrayValidationTest, multi_dimention_decl_init)
{
	std::string code = R"(
	void foo()
	{
		int i[6][2] = {{1, 2}, {3, 4}, {1, 2}, {3, 4}, {1, 2}, {3, 4}};
	}
	)";

	ExpectNoError(code);
}

TEST_F(ArrayValidationTest, multi_dimention_assign)
{
	std::string code = R"(
	int foo()
	{
		int i[2][3];

		i[1][2] = 5;

		return i[1][2];
	}
	)";

	ExpectNoError(code);
}

TEST_F(ArrayValidationTest, array_of_structs_init)
{
	std::string code = R"(
	void foo()
	{
		struct A {int x, y;} a[2] = {{1, 2}, {3, 4}};
	}
	)";

	ExpectNoError(code);
}

TEST_F(ArrayValidationTest, ptr_conversion)
{
	std::string code = R"(
	void foo()
	{
		int a[10];
		int *p = a;
	}
	)";

	ExpectNoError(code);
}

TEST_F(ArrayValidationTest, unknown_len_array)
{
	std::string code = R"(
	void foo()
	{
		int i[] = {1, 2};
	}
	)";

	ExpectNoError(code);
}

TEST_F(ArrayValidationTest, unknown_len_array_global)
{
	std::string code = R"(
	int i[] = {1, 2};
	)";

	ExpectNoError(code);
}

TEST_F(ArrayValidationTest, unknown_len_array_multi_d)
{
	//array containing 3 arrays each of 2 elements
	std::string code = R"(
	void foo()
	{
		int i[][] = {{1, 2}, {3, 4}, {5, 6}};
	}
	)";

	ExpectNoError(code);
}

TEST_F(ArrayValidationTest, err_unknown_len_array_multi_d_oob_1)
{
	//array containing 3 arrays each of 2 elements
	std::string code = R"(
	void foo()
	{
		int i[][] = {{1, 2}, {3, 4}, {5, 6}};

		i[3][1] = 0;
	}
	)";

	ExpectCompilerError(code, ERR_ARRAY_BOUNDS);
}

TEST_F(ArrayValidationTest, err_unknown_len_array_multi_d_oob_2)
{
	//array containing 3 arrays each of 2 elements
	std::string code = R"(
	void foo()
	{
		int i[][] = {{1, 2}, {3, 4}, {5, 6}};

		i[2][2] = 0;
	}
	)";

	ExpectCompilerError(code, ERR_ARRAY_BOUNDS);
}

TEST_F(ArrayValidationTest, err_unknown_len_array_multi_d_mixed_len_init)
{
	std::string code = R"(
	void foo()
	{
		int i[][] = {{1, 2}, {3, 4, 5}, {6, 7}};
	}
	)";

	ExpectCompilerError(code, ERR_INVALID_INIT);
}

TEST_F(ArrayValidationTest, ptr_to_array)
{
	std::string code = R"(
	int foo()
	{
		int i[2] = {1, 2};
		int (*p)[2] = &i;

		return (*p)[1];
	}
	)";

	ExpectNoError(code);
}