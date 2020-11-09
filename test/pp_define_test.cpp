#include "validation_fixture.h"

class PreProcDefineTest : public LexPreProcTest {};

TEST_F(PreProcDefineTest, define)
{
	std::string src = R"(#define TEST int i
TEST;)";

	PreProc(src.c_str());
	ExpectCode("int i;");
}

TEST_F(PreProcDefineTest, define_nested)
{
	std::string src = R"(#define TEST1 int i;
#define TEST TEST1
TEST)";

	PreProc(src.c_str());
	ExpectCode("int i;");
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

TEST_F(PreProcDefineTest, define_nested_recursive_many)
{
	//enounter 'TEST' while expanding 'TEST' obj macro
	std::string src = R"(#define M1 TEST;
#define A a
#define B A
#define C B
#define D C
D)";

	PreProc(src.c_str());
	ExpectCode("a");
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

TEST_F(PreProcDefineTest, self_reference)
{
	std::string src = R"(
#define TEST (4 + TEST)
TEST;
)";

	PreProc(src.c_str());
	ExpectCode("(4 + TEST);");
}

TEST_F(PreProcDefineTest, self_reference_self)
{
	std::string src = R"(
#define TEST TEST
TEST;
)";

	PreProc(src.c_str());
	ExpectCode("TEST;");
}


TEST_F(PreProcDefineTest, two_defs)
{
	std::string src = R"(
#define ONE 1
#define TWO 2
ONE + TWO;
)";

	PreProc(src.c_str());
	ExpectCode("1 + 2;");
}

TEST_F(PreProcDefineTest, self_circ_reference)
{
	std::string src = R"(
#define x (4 + y)
#define y (2 * x)

x;
y;
)";

	PreProc(src.c_str());
	ExpectCode(R"(
(4 + (2 * x));
(2 * (4 + y));
)");
}

/////////////////////////////////////////////

TEST_F(PreProcDefineTest, fn)
{
	std::string src = R"(
#define TEST(A) A

TEST(1);

)";

	PreProc(src.c_str());
	ExpectCode("1;");
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

TEST_F(PreProcDefineTest, fn_macro_in_param)
{
	std::string src = R"(
#define ONE 1
#define TEST(A) A

TEST(ONE);
)";

	PreProc(src.c_str());
	ExpectCode("1;");
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

/*TEST_F(PreProcDefineTest, fn_param_stringize)
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
}*/

TEST_F(PreProcDefineTest, nested_fn)
{
	std::string src = R"(
#define f(a) a + 1

f(f(z));
)";

	PreProc(src.c_str());
	ExpectCode("z + 1 + 1;");
}

TEST_F(PreProcDefineTest, fn_param_double_replace)
{
	std::string src = R"(
#define f(a) a
#define z z[0]

f(z);

)";
	//z is only expanded to z[0] once
	PreProc(src.c_str());
	ExpectCode("z[0];");
}

TEST_F(PreProcDefineTest, fn_nested_with_double_replace)
{
	std::string src = R"(
#define f(a) a
#define z z[0]

f(f(z));
)";

	PreProc(src.c_str());
	ExpectCode("z[0];");
}

TEST_F(PreProcDefineTest, fn_name_replace)
{
	std::string src = R"(
#define x 2
#define f(a) f(x * (a))
#define g f

g(0)
)";
	PreProc(src.c_str());
	ExpectCode("f(2 * (0))");
}

TEST_F(PreProcDefineTest, fn_not_replaced_when_not_a_call)
{
	std::string src = R"(

#define f(a) f(2 * (a))
#define g f

#define t(a) a

t(g)(0);
)";
	//g is replaced with f but does not form a call until t is expanded

	PreProc(src.c_str());
	ExpectCode("f(2 * (0));");
}

TEST_F(PreProcDefineTest, example_3_1)
{
	std::string src = R"(
#define x 3
#define f(a) f(x * (a))
#undef x
#define x 2
#define g f
#define z z[0]
#define h g(~
#define m(a) a(w)
#define w 0,1
#define t(a) a
#define p() int
#define q(x) x

f(y+1) + f(f(z)) % t(t(g)(0) + t)(1);

)";
	
	PreProc(src.c_str());
	ExpectCode("f(2 * (y+1)) + f(2 * (f(2 * (z[0])))) % f(2 * (0)) + t(1);");
}

//TEST_F(PreProcDefineTest, blah)
//{
//	std::string src = R"(
//#define x 3
//#define f(a) f(x * (a))
//#undef x
//#define x 2
//#define g f
//#define z z[0]
//#define h g(~
//#define m(a) a(w)
//#define w 0,1
//#define t(a) a
//#define p() int
//#define q(x) x
//
//h 5);
//)";
//
//	PreProc(src.c_str());
//	ExpectCode("f(2 * (~ 5));");
//	PrintTokens();
//}

TEST_F(PreProcDefineTest, example_3_2)
{
	std::string src = R"(
#define x 3
#define f(a) f(x * (a))
#undef x
#define x 2
#define g f
#define z z[0]
#define h g(~
#define m(a) a(w)
#define w 0,1
#define t(a) a
#define p() int
#define q(x) x

g(x+(3,4)-w);

)";

	PreProc(src.c_str());
	ExpectCode("f(2 * (2+(3,4)-0,1));");
}