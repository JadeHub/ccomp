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

	void Lex(const char* src)
	{
		source_range_t sr;
		sr.ptr = src;
		sr.end = src + strlen(src);

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