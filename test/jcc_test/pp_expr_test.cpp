#include "validation_fixture.h"

#include <optional>

class PreProcExprTest : public LexPreProcTest 
{
public:

	void ExpectEval(const std::string& src, std::optional<uint32_t> expected)
	{
		Lex(src);

		token_range_t range;
		range.start = tokens.start;
		range.end = tok_find_next(tokens.start, tok_eof);

		uint32_t val;
		pp_context_t ctx = { sht_create(128), range.start, range.end };
		pre_proc_eval_expr(&ctx, range, &val);

		if(expected.has_value())
			EXPECT_EQ(val, *expected);
	}
};

/* binary ops */

/* arithmetic*/
TEST_F(PreProcExprTest, mod)
{
	std::string src = "15%10";
	ExpectEval(src, 5);
}

TEST_F(PreProcExprTest, mul)
{
	std::string src = "15*10";
	ExpectEval(src, 150);
}

TEST_F(PreProcExprTest, div)
{
	std::string src = "100/10";
	ExpectEval(src, 10);
}

TEST_F(PreProcExprTest, plus)
{
	std::string src = "5+3";
	ExpectEval(src, 8);
}

TEST_F(PreProcExprTest, plus_minus)
{
	std::string src = "15+3-6";
	ExpectEval(src, 12);
}

TEST_F(PreProcExprTest, precedence)
{
	std::string src = "10+5*2";
	ExpectEval(src, 20);
}

TEST_F(PreProcExprTest, precedence1)
{
	std::string src = "(10+5)*2";
	ExpectEval(src, 30);
}

/* logical */
TEST_F(PreProcExprTest, equal_equal_true)
{
	std::string src = "10 == 10";
	ExpectEval(src, 1);
}

TEST_F(PreProcExprTest, equal_equal_false)
{
	std::string src = "10 == 11";
	ExpectEval(src, 0);
}

/* shifts */
TEST_F(PreProcExprTest, shift_left)
{
	std::string src = "1 << 8";
	ExpectEval(src, 1 << 8);
}

TEST_F(PreProcExprTest, shift_left_right)
{
	std::string src = "(1 << 8) >> 8";
	ExpectEval(src, 1);
}

TEST_F(PreProcExprTest, and)
{
	std::string src = "1 && 0";
	ExpectEval(src, 0);
}

TEST_F(PreProcExprTest, or)
{
	std::string src = "1 || 0";
	ExpectEval(src, 1);
}

TEST_F(PreProcExprTest, bw_and)
{
	std::string src = "255 & 16";
	ExpectEval(src, 255 & 16);
}

/* Unary ops */
TEST_F(PreProcExprTest, exclaim)
{
	std::string src = "!5";
	ExpectEval(src, !5);
}

TEST_F(PreProcExprTest, exclaim2)
{
	std::string src = "!!5";
	ExpectEval(src, !!5);
}

TEST_F(PreProcExprTest, paren_exclaim)
{
	std::string src = "(!5)";
	ExpectEval(src, !5);
}

TEST_F(PreProcExprTest, tilda)
{
	std::string src = "~5";
	ExpectEval(src, ~5);
}

TEST_F(PreProcExprTest, paren)
{
	std::string src = R"(
((5))
)";
	ExpectEval(src, 5);
}

TEST_F(PreProcExprTest, err_overflow)
{
	std::string src = "0xFFFFFFFF << 1";

	ExpectError(ERR_VALUE_OVERFLOW);
	ExpectEval(src, std::nullopt);
}
