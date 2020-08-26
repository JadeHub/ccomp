#include <gtest/gtest.h>

#include <vector>

#include "validation_fixture.h"

extern "C"
{
#include <libcomp/include/lexer.h>
}

using namespace ::testing;

class LexTest : public TestWithErrorHandling
{
public:

	LexTest()
	{
	}

	~LexTest()
	{
	}

	void Lex(const std::string& src)
	{
		source_range_t sr;
		sr.ptr = src.c_str();
		sr.end = sr.ptr + src.length();

		token_t* toks = lex_source(&sr);

		token_t* tok = toks;
		while (tok)
		{
			tokens.push_back(*tok);
			token_t* next = tok->next;
			free(tok);
			tok = next;
		}
	}

	std::vector<token_t> tokens;

	template <typename T>
	void ExpectIntLiteral(const token_t& t, T val)
	{
		EXPECT_EQ(t.kind, tok_num_literal);
		EXPECT_EQ(t.data, (uint32_t)val);
	}

	void ExpectStringLiteral(const token_t& t, const char* expected)
	{
		const char* str = (const char*)t.data;
		EXPECT_STREQ(str, expected);
	}
};

TEST_F(LexTest, foo)
{
	Lex(R"(int foo = 0;)");
}

TEST_F(LexTest, Eof)
{
	std::string code = R"(
	int foo(int a);
	int foo = 5;	)";

	Lex(code);
}

TEST_F(LexTest, Comment)
{
	std::string code = R"(
	int foo //(int a);
	int foo2 = 5;	)";

	Lex(code);
	EXPECT_EQ(tokens[0].kind, tok_int);
	EXPECT_EQ(tokens[1].kind, tok_identifier);
	EXPECT_TRUE(tok_spelling_cmp(&tokens[1], "foo"));

	EXPECT_EQ(tokens[2].kind, tok_int);
	EXPECT_EQ(tokens[3].kind, tok_identifier);
	EXPECT_TRUE(tok_spelling_cmp(&tokens[3], "foo2"));
	EXPECT_EQ(tokens[4].kind, tok_equal);
	EXPECT_EQ(tokens[5].kind, tok_num_literal);
}

TEST_F(LexTest, HexConstant1)
{
	std::string code = R"(int x = 0xFF;)";

	Lex(code);
	ExpectIntLiteral(tokens[3], 0xFF);
}

TEST_F(LexTest, HexConstant2)
{
	std::string code = R"(int x = 0xFf;)";

	Lex(code);
	ExpectIntLiteral(tokens[3], 0xFF);
}

TEST_F(LexTest, HexConstant3)
{
	std::string code = R"(int x = 0x00000;)";

	Lex(code);
	ExpectIntLiteral(tokens[3], 0);
}

TEST_F(LexTest, HexConstant4)
{
	std::string code = R"(int x = 0x00000acb;)";

	Lex(code);
	ExpectIntLiteral(tokens[3], 0xACB);
}

TEST_F(LexTest, HexConstant5)
{
	std::string code = R"(int x = 0x00000acb; int main())";

	Lex(code);
	ExpectIntLiteral(tokens[3], 0xACB);
	EXPECT_EQ(tokens[4].kind, tok_semi_colon);
	EXPECT_EQ(tokens[5].kind, tok_int);
	EXPECT_EQ(tokens[6].kind, tok_identifier);
}

TEST_F(LexTest, ErrCharConstantConstantTooLarge)
{
	std::string code = R"(int x = '\xFF1';)";

	ExpectError(ERR_SYNTAX);
	Lex(code);
}

TEST_F(LexTest, OctalConstant1)
{
	std::string code = R"(int x = 052;)";

	Lex(code);
	ExpectIntLiteral(tokens[3], 42);
}

TEST_F(LexTest, BinaryConstant1)
{
	std::string code = R"(int x = 052;)";

	Lex(code);
	ExpectIntLiteral(tokens[3], 42);
}

TEST_F(LexTest, CharConstant1)
{
	std::string code = R"(int x = 'a';)";

	Lex(code);
	ExpectIntLiteral(tokens[3], 'a');
}

TEST_F(LexTest, CharConstantEsc1)
{
	std::string code = R"(int x = '\t';)";

	Lex(code);
	ExpectIntLiteral(tokens[3], '\t');
}

TEST_F(LexTest, CharConstantNull)
{
	std::string code = R"(int x = '\0';)";

	Lex(code);
	ExpectIntLiteral(tokens[3], 0);
}

TEST_F(LexTest, CharConstantEscHex)
{
	std::string code = R"(int x = '\xFF';)";

	Lex(code);
	ExpectIntLiteral(tokens[3], 0xFF);
}

TEST_F(LexTest, CharConstantEscHex2)
{
	std::string code = R"(int x = '\x0F';)";

	Lex(code);
	ExpectIntLiteral(tokens[3], 0x0F);
}

TEST_F(LexTest, CharConstantEscOctal)
{
	std::string code = R"(int x = '\052';)";

	Lex(code);
	ExpectIntLiteral(tokens[3], 42);
}

TEST_F(LexTest, ErrCharConstantBadEsc)
{
	std::string code = R"(int x = '\q';)";

	ExpectError(ERR_SYNTAX);
	Lex(code);	
}

TEST_F(LexTest, ErrCharConstantEmpty)
{
	std::string code = R"(int x = '';)";

	ExpectError(ERR_SYNTAX);
	Lex(code);
}

TEST_F(LexTest, ErrCharConstantTooLong)
{
	std::string code = R"(int x = '\nr';)";

	ExpectError(ERR_SYNTAX);
	Lex(code);
}

TEST_F(LexTest, ErrCharConstantUnterminated)
{
	std::string code = R"(int x = 'a;)";

	ExpectError(ERR_SYNTAX);
	Lex(code);
}

TEST_F(LexTest, ErrCharConstantUnterminated2)
{
	std::string code = R"(int x = 'a
						';)";

	ExpectError(ERR_SYNTAX);
	Lex(code);
}
/*
TEST_F(LexTest, StringConstant)
{
	std::string code = R"(int x = "abc";)";

	Lex(code);
	ExpectStringLiteral(tokens[3], "abc");
}

TEST_F(LexTest, EmptyStringConstant)
{
	std::string code = R"(int x = "";)";

	Lex(code);
	ExpectStringLiteral(tokens[3], "");
}

TEST_F(LexTest, ErrStringConstantUnterminated)
{
	std::string code = R"(int x = "abc;)";

	ExpectError(ERR_SYNTAX);
	Lex(code);
}

TEST_F(LexTest, ErrStringConstantUnterminated2)
{
	std::string code = R"(int x = "abc;
							")";

	ExpectError(ERR_SYNTAX);
	Lex(code);
}
*/