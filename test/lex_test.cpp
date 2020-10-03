#include "validation_fixture.h"

class LexerTest : public LexTest {};

TEST_F(LexerTest, foo)
{
	Lex(R"(int foo = 0;)");
}

TEST_F(LexerTest, Eof)
{
	std::string code = R"(
	int foo(int a);
	int foo = 5;	)";

	Lex(code);
}

TEST_F(LexerTest, Comment)
{
	std::string code = R"(
	int foo //(int a);
	int foo2 = 5;	)";

	Lex(code);
	EXPECT_EQ(tokens[0].kind, tok_int);
	EXPECT_EQ(tokens[1].kind, tok_identifier);
//	EXPECT_TRUE(tok_spelling_cmp(&tokens[1], "foo"));

	EXPECT_EQ(tokens[2].kind, tok_int);
	EXPECT_EQ(tokens[3].kind, tok_identifier);
	//EXPECT_TRUE(tok_spelling_cmp(&tokens[3], "foo2"));
	EXPECT_EQ(tokens[4].kind, tok_equal);
	EXPECT_EQ(tokens[5].kind, tok_num_literal);
}

TEST_F(LexerTest, HexConstant1)
{
	std::string code = R"(int x = 0xFF;)";

	Lex(code);
	ExpectIntLiteral(tokens[3], 0xFF);
}

TEST_F(LexerTest, HexConstant2)
{
	std::string code = R"(int x = 0xFf;)";

	Lex(code);
	ExpectIntLiteral(tokens[3], 0xFF);
}

TEST_F(LexerTest, HexConstant3)
{
	std::string code = R"(int x = 0x00000;)";

	Lex(code);
	ExpectIntLiteral(tokens[3], 0);
}

TEST_F(LexerTest, HexConstant4)
{
	std::string code = R"(int x = 0x00000acb;)";

	Lex(code);
	ExpectIntLiteral(tokens[3], 0xACB);
}

TEST_F(LexerTest, HexConstant5)
{
	std::string code = R"(int x = 0x00000acb; int main())";

	Lex(code);
	ExpectIntLiteral(tokens[3], 0xACB);
	EXPECT_EQ(tokens[4].kind, tok_semi_colon);
	EXPECT_EQ(tokens[5].kind, tok_int);
	EXPECT_EQ(tokens[6].kind, tok_identifier);
}

TEST_F(LexerTest, ErrCharConstantConstantTooLarge)
{
	std::string code = R"(int x = '\xFF1';)";

	ExpectError(ERR_SYNTAX);
	Lex(code);
}

TEST_F(LexerTest, OctalConstant1)
{
	std::string code = R"(int x = 052;)";

	Lex(code);
	ExpectIntLiteral(tokens[3], 42);
}

TEST_F(LexerTest, BinaryConstant1)
{
	std::string code = R"(int x = 052;)";

	Lex(code);
	ExpectIntLiteral(tokens[3], 42);
}

TEST_F(LexerTest, CharConstant1)
{
	std::string code = R"(int x = 'a';)";

	Lex(code);
	ExpectIntLiteral(tokens[3], 'a');
}

TEST_F(LexerTest, CharConstantEsc1)
{
	std::string code = R"(int x = '\t';)";

	Lex(code);
	ExpectIntLiteral(tokens[3], '\t');
}

TEST_F(LexerTest, CharConstantNull)
{
	std::string code = R"(int x = '\0';)";

	Lex(code);
	ExpectIntLiteral(tokens[3], 0);
}

TEST_F(LexerTest, CharConstantEscHex)
{
	std::string code = R"(int x = '\xFF';)";

	Lex(code);
	ExpectIntLiteral(tokens[3], 0xFF);
}

TEST_F(LexerTest, CharConstantEscHex2)
{
	std::string code = R"(int x = '\x0F';)";

	Lex(code);
	ExpectIntLiteral(tokens[3], 0x0F);
}

TEST_F(LexerTest, CharConstantEscOctal)
{
	std::string code = R"(int x = '\052';)";

	Lex(code);
	ExpectIntLiteral(tokens[3], 42);
}

TEST_F(LexerTest, ErrCharConstantBadEsc)
{
	std::string code = R"(int x = '\q';)";

	ExpectError(ERR_SYNTAX);
	Lex(code);	
}

TEST_F(LexerTest, ErrCharConstantEmpty)
{
	std::string code = R"(int x = '';)";

	ExpectError(ERR_SYNTAX);
	Lex(code);
}

TEST_F(LexerTest, ErrCharConstantTooLong)
{
	std::string code = R"(int x = '\nr';)";

	ExpectError(ERR_SYNTAX);
	Lex(code);
}

TEST_F(LexerTest, ErrCharConstantUnterminated)
{
	std::string code = R"(int x = 'a;)";

	ExpectError(ERR_SYNTAX);
	Lex(code);
}

TEST_F(LexerTest, ErrCharConstantUnterminated2)
{
	std::string code = R"(int x = 'a
						';)";

	ExpectError(ERR_SYNTAX);
	Lex(code);
}

TEST_F(LexerTest, StringConstant)
{
	std::string code = R"(char* c = "abc";)";

	Lex(code);
	ExpectStringLiteral(tokens[4], "abc");
}

TEST_F(LexerTest, EmptyStringConstant)
{
	std::string code = R"(char* c = "";)";

	Lex(code);
	ExpectStringLiteral(tokens[4], "");
}

TEST_F(LexerTest, ErrStringConstantUnterminated)
{
	std::string code = R"(char* c = "abc;)";

	ExpectError(ERR_SYNTAX);
	Lex(code);
}

TEST_F(LexerTest, ErrStringConstantUnterminated2)
{
	std::string code = R"(char* c = "abc;
							")";

	ExpectError(ERR_SYNTAX);
	Lex(code);
}



