#include "validation_fixture.h"

extern "C"
{
#include <libcomp/include/sema_internal.h>
}

class ConstExprEvalTest : public CompilerTest
{
public:
	int_val_t Eval(const std::string& code)
	{
		SetSource(code);
		Lex();

		parse_init(mTokens);
		ast_expression_t* expr = parse_constant_expression();

		int_val_t iv = sema_eval_constant_expr(expr);

		return iv;
	}

	void EvalExpectUnsigned(uint64_t val, const std::string& expr)
	{
		int_val_t result = Eval(expr);
		EXPECT_EQ(false, result.is_signed);
		EXPECT_EQ(val, result.uint64);
	}

	void EvalExpectSigned(int64_t val, const std::string& expr)
	{
		int_val_t result = Eval(expr);
		EXPECT_EQ(true, result.is_signed);
		EXPECT_EQ(val, result.int64);
	}
};

TEST_F(ConstExprEvalTest, Addition)
{
	EvalExpectUnsigned(2, "1 + 1");
	EvalExpectSigned(-9, "-10 + 1");
	EvalExpectUnsigned(0, "-1 + 1");
	uint64_t t = 0xFFFFFFFFLL + 1;
	EvalExpectUnsigned(t, "0xFFFFFFFF + 1");
}

TEST_F(ConstExprEvalTest, Subtraction)
{
	EvalExpectUnsigned(2, "12 - 10");
	EvalExpectSigned(-11, "-10 - 1");
	EvalExpectSigned(-2, "5 - 7");
	EvalExpectUnsigned(0, "1 - 1");
}

TEST_F(ConstExprEvalTest, Multiplication)
{
	EvalExpectUnsigned(20, "2 * 10");
	EvalExpectSigned(-20, "2 * -10");
	EvalExpectSigned(-20, "-2 * 10");
}

TEST_F(ConstExprEvalTest, Division)
{
	EvalExpectUnsigned(2, "12 / 6");
	EvalExpectSigned(-2, "-12 / 6");
	EvalExpectSigned(-2, "12 / -6");
}

TEST_F(ConstExprEvalTest, Precidence)
{
	EvalExpectUnsigned(22, "2 * (10 + 1)");
	EvalExpectUnsigned(21, "2 * 10 + 1");
}

TEST_F(ConstExprEvalTest, Modulus)
{
	EvalExpectUnsigned(2, "12 % 10");
	EvalExpectUnsigned(0, "12 % (5 + 1)");
}

TEST_F(ConstExprEvalTest, Sizeof)
{
	EvalExpectUnsigned(1, "sizeof(char)");
	EvalExpectUnsigned(2, "sizeof(short)");
	EvalExpectUnsigned(4, "sizeof(int)");
	EvalExpectUnsigned(8, "sizeof(int) * sizeof(short)");
	EvalExpectSigned(-8, "sizeof(int) * -sizeof(short)");
}

TEST_F(ConstExprEvalTest, LeftShift)
{
	EvalExpectUnsigned(1<<5, "1 << 5");	
}

TEST_F(ConstExprEvalTest, AndBothFalse)
{
	EvalExpectUnsigned(0, "0 && 0");
}

TEST_F(ConstExprEvalTest, AndFirstFalse)
{
	EvalExpectUnsigned(0, "0 && 5");
}

TEST_F(ConstExprEvalTest, AndSecondFalse)
{
	EvalExpectUnsigned(0, "5 && 0");
}

TEST_F(ConstExprEvalTest, AndTrue)
{
	EvalExpectUnsigned(1, "5 && 1");
}

TEST_F(ConstExprEvalTest, OrBothTrue)
{
	EvalExpectUnsigned(1, "1 || 1");
}

TEST_F(ConstExprEvalTest, OrFirstTrue)
{
	EvalExpectUnsigned(1, "5 || 0");
}

TEST_F(ConstExprEvalTest, OrSecondTrue)
{
	EvalExpectUnsigned(1, "0 || 5");
}

TEST_F(ConstExprEvalTest, OrFalse)
{
	EvalExpectUnsigned(0, "0 || 0");
}

TEST_F(ConstExprEvalTest, Tilda)
{
	EvalExpectUnsigned(~(1 + 1), "~(1 + 1)");
}

