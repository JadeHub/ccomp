#include "validation_fixture.h"

class LexLineContTest : public LexTest {};

TEST_F(LexLineContTest, IntConstant)
{
	std::string code = R"(int x = 123\
4;)";

	Lex(code);
	ExpectIntLiteral(tokens[3], 1234);
}

TEST_F(LexLineContTest, StringConstant)
{
	std::string code = R"(char* c = "hell\
o";)";

	ExpectNoError();
	Lex(code);
	ExpectStringLiteral(tokens[4], "hello");
}

TEST_F(LexLineContTest, BinOp)
{
	std::string code = R"(a !\
= b)";

	ExpectNoError();
	Lex(code);

	EXPECT_EQ(tok_identifier, tokens[0].kind);
	EXPECT_EQ(tok_exclaimequal, tokens[1].kind);
	EXPECT_EQ(tok_identifier, tokens[2].kind);
	EXPECT_EQ(tok_eof, tokens[3].kind);
}

TEST_F(LexLineContTest, Keyword)
{
	std::string code = R"(ret\
urn b;)";

	ExpectNoError();
	Lex(code);
	EXPECT_EQ(tok_return, tokens[0].kind);
	EXPECT_EQ(tok_identifier, tokens[1].kind);
	EXPECT_EQ(tok_semi_colon, tokens[2].kind);
	EXPECT_EQ(tok_eof, tokens[3].kind);
}

TEST_F(LexLineContTest, Identifier)
{
	std::string code = R"(func\
tion();)";

	ExpectNoError();
	Lex(code);
	EXPECT_EQ(tok_identifier, tokens[0].kind);
	EXPECT_EQ(tok_l_paren, tokens[1].kind);
	EXPECT_EQ(tok_r_paren, tokens[2].kind);
	EXPECT_EQ(tok_semi_colon, tokens[3].kind);
	EXPECT_EQ(tok_eof, tokens[4].kind);
}