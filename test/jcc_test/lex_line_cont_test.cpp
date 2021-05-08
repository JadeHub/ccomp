#include "validation_fixture.h"

class LexLineContTest : public LexTest {};

TEST_F(LexLineContTest, IntConstant)
{
	std::string code = R"(int x = 123\
4;
char c;)";

	Lex(code);
	ExpectIntLiteral(GetToken(3), 1234);
	
	EXPECT_EQ(tok_int, GetToken(0)->kind);
	EXPECT_TRUE(GetToken(0)->flags & TF_START_LINE);
	
	EXPECT_EQ(tok_identifier, GetToken(1)->kind);
	EXPECT_FALSE(GetToken(1)->flags & TF_START_LINE);

	EXPECT_EQ(tok_equal, GetToken(2)->kind);
	EXPECT_FALSE(GetToken(2)->flags & TF_START_LINE);

	EXPECT_EQ(tok_num_literal, GetToken(3)->kind);
	EXPECT_FALSE(GetToken(3)->flags & TF_START_LINE);

	EXPECT_EQ(tok_semi_colon, GetToken(4)->kind);
	EXPECT_FALSE(GetToken(4)->flags & TF_START_LINE);

	EXPECT_EQ(tok_char, GetToken(5)->kind);
	EXPECT_TRUE(GetToken(5)->flags & TF_START_LINE);

	EXPECT_EQ(tok_semi_colon, GetToken(4)->kind);
	EXPECT_FALSE(GetToken(4)->flags & TF_START_LINE);
}

TEST_F(LexLineContTest, CharConstant)
{
	std::string code = R"(char c = 'c\
';)";

	Lex(code);
	ExpectIntLiteral(GetToken(3), 'c');

	ExpectTokTypes({
			tok_char,
			tok_identifier,
			tok_equal,
			tok_num_literal,
			tok_semi_colon,
			tok_eof });
}

TEST_F(LexLineContTest, StringConstant)
{
	std::string code = R"(char* c = "hell\
o";)";

	ExpectNoError();
	Lex(code);
	ExpectStringLiteral(GetToken(4), "hello");

	ExpectTokTypes({
			tok_char,
			tok_star,
			tok_identifier,
			tok_equal,
			tok_string_literal,
			tok_semi_colon,
			tok_eof});
}

TEST_F(LexLineContTest, BinOp)
{
	std::string code = R"(a !\
= b)";

	ExpectNoError();
	Lex(code);
	
	ExpectTokTypes({
			tok_identifier,
			tok_exclaimequal,
			tok_identifier,
			tok_eof });
}

TEST_F(LexLineContTest, Keyword)
{
	std::string code = R"(ret\
urn b;)";

	ExpectNoError();
	Lex(code);

	ExpectTokTypes({
			tok_return,
			tok_identifier,
			tok_semi_colon,
			tok_eof });
}

TEST_F(LexLineContTest, Identifier)
{
	std::string code = R"(func\
tion();)";

	ExpectNoError();
	Lex(code);

	ExpectTokTypes({
			tok_identifier,
			tok_l_paren,
			tok_r_paren,
			tok_semi_colon,
			tok_eof });
}

TEST_F(LexLineContTest, tok_spelling_len)
{
	Lex(R"(ab\
c)");

	ExpectTokTypes({ tok_identifier, tok_eof });
	EXPECT_EQ(3, tok_spelling_len(GetToken(0)));

}