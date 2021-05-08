#include "validation_fixture.h"

class GlobalVarsValidationTest : public ValidationTest {};

TEST_F(GlobalVarsValidationTest, err_var_shadows_fn)
{
	std::string code = R"(
	int foo(int a);
	int foo = 5;
	)";

	ExpectError(code, ERR_DUP_SYMBOL);
}

TEST_F(GlobalVarsValidationTest, err_dup_definition)
{
	std::string code = R"(
	int foo = 4;
	int foo = 5;)";

	ExpectError(code, ERR_DUP_SYMBOL);
}

TEST_F(GlobalVarsValidationTest, err_change_type)
{
	std::string code = R"(
	int foo = 4;
	short foo;	)";

	ExpectError(code, ERR_DUP_SYMBOL);
}

TEST_F(GlobalVarsValidationTest, err_change_type2)
{
	std::string code = R"(
	int foo = 4;
	struct B{int b;} foo;	)";

	ExpectError(code, ERR_DUP_SYMBOL);
}

TEST_F(GlobalVarsValidationTest, err_struct_redefined)
{
	std::string code = R"(
	struct B{int b;} foo;
	struct B{int b;} foo1;	)";

	ExpectError(code, ERR_DUP_TYPE_DEF);
}

TEST_F(GlobalVarsValidationTest, err_struct_incomplete)
{
	std::string code = R"(
	struct B foo;
	)";

	ExpectError(code, ERR_TYPE_INCOMPLETE);
}

TEST_F(GlobalVarsValidationTest, struct_referenced)
{
	std::string code = R"(
	struct B{int b;} foo;
	struct B foo1;
	)";

	ExpectNoError(code);
}

TEST_F(GlobalVarsValidationTest, struct_init)
{
	std::string code = R"(
	struct B{int b; char* s; char c;} foo = {1200, "hello", 15};
	)";

	ExpectNoError(code);
}

TEST_F(GlobalVarsValidationTest, pointer_init)
{
	std::string code = R"(
	char* s = "Hello";
	)";

	ExpectNoError(code);
}

TEST_F(GlobalVarsValidationTest, invalid_init)
{
	std::string code = R"(
	int fooA = 4;
	int foo = fooA;	)";

	ExpectError(code, ERR_INITIALISER_NOT_CONST);
}

TEST_F(GlobalVarsValidationTest, global_fn_shadows_var)
{
	std::string code = R"(
	int foo = 5;	
	int foo(int a);)";

	ExpectError(code, ERR_DUP_SYMBOL);
}

TEST_F(GlobalVarsValidationTest, overflow_uint)
{
	std::string code = R"(
	unsigned int foo = 0xFFFFFFFF + 1;
	)";

	ExpectError(code, ERR_INCOMPATIBLE_TYPE);
}

TEST_F(GlobalVarsValidationTest, overflow_uchar)
{
	std::string code = R"(
	unsigned char foo = 0xFF + 1;
	)";

	ExpectError(code, ERR_INCOMPATIBLE_TYPE);
}

TEST_F(GlobalVarsValidationTest, overflow_char)
{
	std::string code = R"(
	char foo = 0xFF;
	)";

	ExpectError(code, ERR_INCOMPATIBLE_TYPE);
}

TEST_F(GlobalVarsValidationTest, valid_char)
{
	std::string code = R"(
	char foo = 10;
	)";

	ExpectNoError(code);
}

TEST_F(GlobalVarsValidationTest, init_ptr)
{
	std::string code = R"(
	char* foo = "hello world!";
	)";

	ExpectNoError(code);
}