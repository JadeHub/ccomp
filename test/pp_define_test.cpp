#include "validation_fixture.h"

class PreProcDefineTest : public LexPreProcTest {};

TEST_F(PreProcDefineTest, define)
{
	std::string src = R"(#define TEST int i;
TEST)";

	PreProc(src.c_str());
	ExpectTokTypes({ tok_int,
		tok_identifier,
		tok_semi_colon,
		tok_eof });
}

TEST_F(PreProcDefineTest, define_nested)
{
	std::string src = R"(#define TEST1 int i;
#define TEST TEST1
TEST)";

	PreProc(src.c_str());
	ExpectTokTypes({ tok_int,
		tok_identifier,
		tok_semi_colon,
		tok_eof });
}

TEST_F(PreProcDefineTest, define_nested_recursive)
{
	//enounter 'TEST' while expanding 'TEST' obj macro
	std::string src = R"(#define M1 TEST;
#define TEST1 M1
#define TEST TEST1
TEST)";

	PreProc(src.c_str());
	ExpectTokTypes({tok_identifier,
		tok_semi_colon,
		tok_eof });
}

TEST_F(PreProcDefineTest, err_missing_whitespace)
{
	std::string src = R"(
#define TEST+12
TEST
)";

	ExpectError(ERR_SYNTAX);
	PreProc(src.c_str());
}

TEST_F(PreProcDefineTest, correct_whitespace)
{
	std::string src = R"(
#define TEST +12
TEST
)";

	PreProc(src.c_str());
	ExpectTokTypes({ tok_plus,
		tok_num_literal,
		tok_eof });
}

TEST_F(PreProcDefineTest, err_redefinition)
{
	std::string src = R"(
#define TEST
#define TEST int
)";

	ExpectError(ERR_SYNTAX);
	PreProc(src.c_str());
}

TEST_F(PreProcDefineTest, duplicate_definition)
{
	std::string src = R"(
#define TEST int i = 7;
#define TEST int i = 7;
TEST
)";

	PreProc(src.c_str());
	ExpectTokTypes({ tok_int,
		tok_identifier,
		tok_equal,
		tok_num_literal,
		tok_semi_colon,
		tok_eof });
}

TEST_F(PreProcDefineTest, duplicate_definition_empty)
{
	std::string src = R"(
#define TEST
#define TEST  
)";

	PreProc(src.c_str());
	ExpectTokTypes({ 
		tok_eof });
}

TEST_F(PreProcDefineTest, fn_start_of_line)
{
	std::string src = R"(
#define TEST(A, B, CDE) A + B + CDE

TEST(1 + 3, 2, 3);

)";

	PreProc(src.c_str());
	ExpectCode("1 + 3 + 2 + 3;");
}

TEST_F(PreProcDefineTest, fn_mid_line)
{
	std::string src = R"(
#define TEST(A, B, CDE) A + B + CDE

2 - TEST(1 + 3, 2, 3);

)";

	PreProc(src.c_str());
	ExpectCode("2 - 1 + 3 + 2 + 3;");
}

//Within the sequence of preprocessing tokens making up an invocation of a function-like macro,
//new-line is considered a normal white-space character
TEST_F(PreProcDefineTest, fn_multi_line)
{
	std::string src = R"(
#define TEST(A, B, CDE) A + B + CDE

TEST(1 +
3,
2, 
3);

)";

	PreProc(src.c_str());
	ExpectCode("1 + 3 + 2 + 3;");
}

TEST_F(PreProcDefineTest, fn_macro_in_params)
{
	std::string src = R"(
#define ONE 1
#define TWO 2
#define THREE 3
#define TEST(A, B, CDE) A + B + CDE

TEST(ONE, TWO, THREE);
)";

	PreProc(src.c_str());
	ExpectCode("1 + 2 + 3;");
}

TEST_F(PreProcDefineTest, fn_recursive_macro_in_params1)
{
	std::string src = R"(
#define ONE 1
#define TWO ONE

#define TEST(A, B) A + B

TEST(TWO, ONE);
)";

	PreProc(src.c_str());
	ExpectCode("1 + 1;");
}

TEST_F(PreProcDefineTest, fn_param_stringize)
{
	std::string src = R"(
#define TEST(A) #A

TEST(hello("bob");)
)";

	PreProc(src.c_str());
	std::string expected = R"(
"hello(\"bob\");"
)";
	ExpectCode(expected);
}

TEST_F(PreProcDefineTest, fn_param_stringize_num)
{
	std::string src = R"(
#define TEST(A) #A

TEST(12\
345)
)";

	PreProc(src.c_str());
	std::string expected = R"(
"12345"
)";
	ExpectCode(expected);
}

TEST_F(PreProcDefineTest, fn_param_stringize_multi_line)
{
	std::string src = R"(
#define TEST(A) #A

TEST("hel\
lo")
)";

	PreProc(src.c_str());
	std::string expected = R"(
"\"hello\""
)";
	ExpectCode(expected);
}