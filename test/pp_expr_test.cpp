#include "validation_fixture.h"

class PreProcExprTest : public LexPreProcTest 
{
public:

	void ExpectEval(const std::string& src, uint32_t expected)
	{
		Lex(src);

		token_range_t range;
		range.start = tokens;
		range.end = tok_find_next(tokens, tok_eof);

		uint32_t val;
		pp_context_t ctx = { sht_create(128), range.start, range.end };
		pre_proc_eval_expr(&ctx, range, &val);

		EXPECT_EQ(val, expected);
	}
};

/* binary ops */

/* arithmetic*/
TEST_F(PreProcExprTest, mod)
{
	std::string src = R"(
15%10
)";
	ExpectEval(src, 5);
}

TEST_F(PreProcExprTest, mul)
{
	std::string src = R"(
15*10
)";
	ExpectEval(src, 150);
}

TEST_F(PreProcExprTest, div)
{
	std::string src = R"(
100/10
)";
	ExpectEval(src, 10);
}

TEST_F(PreProcExprTest, plus)
{
	std::string src = R"(
5+3
)";
	ExpectEval(src, 8);
}

TEST_F(PreProcExprTest, plus_minus)
{
	std::string src = R"(
15+3-6
)";
	ExpectEval(src, 12);
}

TEST_F(PreProcExprTest, precedence)
{
	std::string src = R"(
10+5*2
)";
	ExpectEval(src, 20);
}

TEST_F(PreProcExprTest, precedence1)
{
	std::string src = R"(
(10+5)*2
)";
	ExpectEval(src, 30);
}

/* logical */
TEST_F(PreProcExprTest, equal_equal_true)
{
	std::string src = R"(
10 == 10
)";
	ExpectEval(src, 1);
}

TEST_F(PreProcExprTest, equal_equal_false)
{
	std::string src = R"(
10 == 11
)";
	ExpectEval(src, 0);
}

/* shifts */
TEST_F(PreProcExprTest, shift_left)
{
	std::string src = R"(
1 << 8
)";
	ExpectEval(src, 1 << 8);
}

TEST_F(PreProcExprTest, shift_left_right)
{
	std::string src = R"(
(1 << 8) >> 8
)";
	ExpectEval(src, 1);
}

TEST_F(PreProcExprTest, and)
{
	std::string src = R"(
1 && 0
)";
	ExpectEval(src, 0);
}

TEST_F(PreProcExprTest, or)
{
	std::string src = R"(
1 || 0
)";
	ExpectEval(src, 1);
}

TEST_F(PreProcExprTest, bw_and)
{
	std::string src = R"(
255 & 16
)";
	ExpectEval(src, 255 & 16);
}

/* Unary ops */
TEST_F(PreProcExprTest, exclaim)
{
	std::string src = R"(
!5
)";
	ExpectEval(src, !5);
}

TEST_F(PreProcExprTest, exclaim2)
{
	std::string src = R"(
!!5
)";
	ExpectEval(src, !!5);
}

TEST_F(PreProcExprTest, paren_exclaim)
{
	std::string src = R"(
(!5)
)";
	ExpectEval(src, !5);
}

TEST_F(PreProcExprTest, tilda)
{
	std::string src = R"(
~5
)";
	ExpectEval(src, ~5);
}

TEST_F(PreProcExprTest, paren)
{
	std::string src = R"(
((5))
)";
	ExpectEval(src, 5);
}
