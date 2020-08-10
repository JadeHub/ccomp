#include <gtest/gtest.h>

#include <vector>

extern "C"
{
#include <libcomp/include/lexer.h>
}

using namespace ::testing;

class LexTest : public Test
{
public:

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
		EXPECT_EQ(t.data, (void*)val);
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
