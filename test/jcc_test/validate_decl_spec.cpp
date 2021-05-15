#include "validation_fixture.h"

class DeclSpecValidationTest : public CompilerTest {};

TEST_F(DeclSpecValidationTest, void_valid)
{
	std::string code = R"(
	void foo()
	{	
	}
	)";

	ExpectNoError(code);
}

TEST_F(DeclSpecValidationTest, err_void_int)
{
	std::string code = R"(
	void int foo()
	{	
	}
	)";

	ExpectCompilerError(code, ERR_SYNTAX);
}

TEST_F(DeclSpecValidationTest, err_void_short)
{	std::string code = R"(
	void short foo()
	{	
	}
	)";

	ExpectCompilerError(code, ERR_SYNTAX);
}

TEST_F(DeclSpecValidationTest, short_valid)
{
	std::string code = R"(
	short foo;
	)";

	ExpectNoError(code);
}

TEST_F(DeclSpecValidationTest, short_int_valid)
{
	std::string code = R"(
	short int foo;
	)";

	ExpectNoError(code);
}

TEST_F(DeclSpecValidationTest, ushort_int_valid)
{
	std::string code = R"(
	unsigned short int foo;
	)";

	ExpectNoError(code);
}

TEST_F(DeclSpecValidationTest, int_valid)
{
	std::string code = R"(
	int foo;
	)";

	ExpectNoError(code);
}

TEST_F(DeclSpecValidationTest, long_int_valid)
{
	std::string code = R"(
	long int foo;
	)";

	ExpectNoError(code);
}

TEST_F(DeclSpecValidationTest, unsigned_long_int_valid)
{
	std::string code = R"(
	unsigned long int foo;
	)";

	ExpectNoError(code);
}

TEST_F(DeclSpecValidationTest, signed_long_int_valid)
{
	std::string code = R"(
	signed long int foo;
	)";

	ExpectNoError(code);
}

TEST_F(DeclSpecValidationTest, signed_valid)
{
	std::string code = R"(
	signed foo;
	)";

	ExpectNoError(code);
}

TEST_F(DeclSpecValidationTest, unsigned_valid)
{
	std::string code = R"(
	unsigned foo;
	)";

	ExpectNoError(code);
}

/*TEST_F(DeclSpecValidationTest, long_long_valid)
{
	std::string code = R"(
	long long foo;
	)";

	ExpectNoError(code);
}

TEST_F(DeclSpecValidationTest, long_long_int_valid)
{
	std::string code = R"(
	long long int foo;
	)";

	ExpectNoError(code);
}

TEST_F(DeclSpecValidationTest, unsigned_long_long_int_valid)
{
	std::string code = R"(
	unsigned long long int foo;
	)";

	ExpectNoError(code);
}

TEST_F(DeclSpecValidationTest, signed_long_long_int_valid)
{
	std::string code = R"(
	signed long long int foo;
	)";

	ExpectNoError(code);
}*/

TEST_F(DeclSpecValidationTest, struct_valid)
{
	std::string code = R"(
	struct foo;
	)";

	ExpectNoError(code);
}

TEST_F(DeclSpecValidationTest, union_valid)
{
	std::string code = R"(
	union foo;
	)";

	ExpectNoError(code);
}

TEST_F(DeclSpecValidationTest, err_int_char)
{
	std::string code = R"(
	int char foo;
	)";

	ExpectCompilerError(code, ERR_SYNTAX);
}

TEST_F(DeclSpecValidationTest, err_char_char)
{
	std::string code = R"(
	char char foo;
	)";

	ExpectCompilerError(code, ERR_SYNTAX);
}

TEST_F(DeclSpecValidationTest, err_short_char)
{
	std::string code = R"(
	short char foo;
	)";

	ExpectCompilerError(code, ERR_SYNTAX);
}

TEST_F(DeclSpecValidationTest, err_int_int)
{
	std::string code = R"(
	int int foo;
	)";

	ExpectCompilerError(code, ERR_SYNTAX);
}

TEST_F(DeclSpecValidationTest, err_int_struct)
{
	std::string code = R"(
	int struct foo;
	)";

	ExpectCompilerError(code, ERR_SYNTAX);
}

TEST_F(DeclSpecValidationTest, err_struct_void)
{
	std::string code = R"(
	struct void foo;
	)";

	ExpectCompilerError(code, ERR_SYNTAX);
}

TEST_F(DeclSpecValidationTest, err_unsigned_signed)
{
	std::string code = R"(
	unsigned signed foo;
	)";

	ExpectCompilerError(code, ERR_SYNTAX);
}

TEST_F(DeclSpecValidationTest, err_struct_union)
{
	std::string code = R"(
	struct union foo;
	)";

	ExpectCompilerError(code, ERR_SYNTAX);
}

TEST_F(DeclSpecValidationTest, err_invalid_type)
{
	std::string code = R"(
	iint i;
	)";

	ExpectCompilerError(code, ERR_SYNTAX);
}