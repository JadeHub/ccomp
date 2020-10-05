#include "validation_fixture.h"

TEST_F(LexPreProcTest, err_include_extra_toks_before_eol)
{
	std::string src = R"(#include "stdio.h" int i;)";

	ExpectError(ERR_SYNTAX);

	PreProc(src.c_str());
}

TEST_F(LexPreProcTest, err_include_unterminated)
{
	std::string src = R"(#include "stdio.h
int i;)";

	ExpectError(ERR_SYNTAX);

	PreProc(src.c_str());
}

TEST_F(LexPreProcTest, include1)
{
	std::string stdio = R"(void fn();)";
	std::string src = R"(#include "stdio.h"
void main() {})";
	ExpectFileLoad("stdio.h", stdio);

	PreProc(src.c_str());
	ExpectTokTypes({ tok_void,
		tok_identifier,
		tok_l_paren,
		tok_r_paren,
		tok_semi_colon,
		tok_void,
		tok_identifier,
		tok_l_paren,
		tok_r_paren,
		tok_l_brace,
		tok_r_brace,
		tok_eof
		});
}

TEST_F(LexPreProcTest, include2)
{
	std::string stdio = R"(void fn();)";
	std::string src = R"(#include <stdio.h>)";

	ExpectFileLoad("stdio.h", stdio);

	PreProc(src.c_str());
	ExpectTokTypes({ tok_void,
		tok_identifier,
		tok_l_paren,
		tok_r_paren,
		tok_semi_colon,
		tok_eof
		});
}

TEST_F(LexPreProcTest, include_recursive)
{
	std::string inc1 = R"(int x;)";
	std::string inc2 = R"(#include "inc1.h"
void fn();)";
	std::string src = R"(#include "inc2.h")";

	ExpectFileLoad("inc1.h", inc1);
	ExpectFileLoad("inc2.h", inc2);

	PreProc(src.c_str());
	ExpectTokTypes({ tok_int,
		tok_identifier,
		tok_semi_colon,
		tok_void,
		tok_identifier,
		tok_l_paren,
		tok_r_paren,
		tok_semi_colon,
		tok_eof
		});
}

TEST_F(LexPreProcTest, include_two_files)
{
	std::string inc1 = R"(int x;)";
	std::string inc2 = R"(void fn();)";
	std::string src = R"(#include "inc1.h"
#include "inc2.h")";

	ExpectFileLoad("inc1.h", inc1);
	ExpectFileLoad("inc2.h", inc2);

	PreProc(src.c_str());
	ExpectTokTypes({ tok_int,
		tok_identifier,
		tok_semi_colon,
		tok_void,
		tok_identifier,
		tok_l_paren,
		tok_r_paren,
		tok_semi_colon,
		tok_eof
		});
}


