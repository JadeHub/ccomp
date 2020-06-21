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
};

TEST_F(LexTest, foo)
{
	//Lex(R"(int main() {})");
	Lex(R"(int foo = 0;)");
}

TEST_F(LexTest, Eof)
{
	std::string code = R"(
	int foo(int a);
	int foo = 5;	)";

	Lex(code);
}