#include "validation_fixture.h"

class GlobalFnsValidationTest : public ValidationTest {};

TEST_F(GlobalFnsValidationTest, err_fn_shadows_var)
{
	std::string code = R"(
	int foo = 5;
	int foo(int a);
	)";

	ExpectError(code, ERR_DUP_SYMBOL);
}

TEST_F(GlobalFnsValidationTest, err_dup_definition)
{
	std::string code = R"(
	int foo(int a) {return 2;}
	int foo(int a) {return 2;}
	)";

	ExpectError(code, ERR_DUP_SYMBOL);
}

TEST_F(GlobalFnsValidationTest, err_incomplete_ret_type_definition)
{
	std::string code = R"(
	struct B foo(int a) {return 1;}
	)";

	ExpectError(code, ERR_TYPE_INCOMPLETE);
}

TEST_F(GlobalFnsValidationTest, err_incomplete_param_type)
{
	std::string code = R"(
	int foo(struct B a) {return 1;}
	)";

	ExpectError(code, ERR_TYPE_INCOMPLETE);
}

TEST_F(GlobalFnsValidationTest, err_diff_ret_type)
{
	std::string code = R"(
	int foo(void);
	void foo(void);
	)";

	ExpectError(code, ERR_INVALID_PARAMS);
}

TEST_F(GlobalFnsValidationTest, err_diff_param_type)
{
	std::string code = R"(
	int foo(struct A{int a;} aa);
	int foo(int);
	)";

	ExpectError(code, ERR_INVALID_PARAMS);
}

TEST_F(GlobalFnsValidationTest, err_diff_param_type2)
{
	std::string code = R"(
	int foo(int, struct A{int a;} aa);
	int foo(int, int);
	)";

	ExpectError(code, ERR_INVALID_PARAMS);
}

TEST_F(GlobalFnsValidationTest, err_void_param1)
{
	std::string code = R"(
	int foo(void, int);
	)";

	ExpectError(code, ERR_INVALID_PARAMS);
}

TEST_F(GlobalFnsValidationTest, err_void_param2)
{
	std::string code = R"(
	int foo(int, void);
	)";

	ExpectError(code, ERR_INVALID_PARAMS);
}

TEST_F(GlobalFnsValidationTest, void_param)
{
	std::string code = R"(
	int foo(void);
	int foo();
	)";

	ExpectNoError(code);
}

TEST_F(GlobalFnsValidationTest, struct_param)
{
	std::string code = R"(
	struct A {int b;} foo(struct A a);	
	)";

	ExpectNoError(code);
}

TEST_F(GlobalFnsValidationTest, struct_param2)
{
	std::string code = R"(
	struct A {int b;};
	struct A foo(struct A a);	
	)";

	ExpectNoError(code);
}

TEST_F(GlobalFnsValidationTest, incomplete_ret_type_decl)
{
	std::string code = R"(
	struct B foo(int a);
	)";

	ExpectNoError(code);
}

TEST_F(GlobalFnsValidationTest, incomplete_param_type_decl)
{
	std::string code = R"(
	int foo(struct B a);
	)";

	ExpectNoError(code);
}

TEST_F(GlobalFnsValidationTest, definition_then_decl)
{
	std::string code = R"(
	int foo(int a) {return 2;}
	int foo(int a);
	)";

	ExpectNoError(code);
}

TEST_F(GlobalFnsValidationTest, decl_then_definition)
{
	std::string code = R"(
	int foo(int a);
	int foo(int a) {return 2;}
	)";

	ExpectNoError(code);
}

TEST_F(GlobalFnsValidationTest, decl_decl_then_definition)
{
	std::string code = R"(
	int foo(int a);
	int foo(int a);
	int foo(int a) {return 2;}
	)";

	ExpectNoError(code);
}