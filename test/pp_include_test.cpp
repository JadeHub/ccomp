#if 0

#include "validation_fixture.h"

class PreProcIncludeTest : public LexPreProcTest 
{
public:

	const std::string stdio_code = "void fn();";
	const std::string inc_code = R"(#include "stdio.h"
void main() {})";

	const std::vector<tok_kind> expected_toks = 
	{ 
		tok_void,
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
	};
};

TEST_F(PreProcIncludeTest, err_include_extra_toks_before_eol)
{
	std::string src = R"(#include "stdio.h" int i;)";

	ExpectError(ERR_SYNTAX);

	PreProc(src.c_str());
}

TEST_F(PreProcIncludeTest, err_include_unterminated)
{
	std::string src = R"(#include "stdio.h
int i;)";

	ExpectError(ERR_SYNTAX);

	PreProc(src.c_str());
}

TEST_F(PreProcIncludeTest, include1)
{
	ExpectFileLoad("stdio.h", stdio_code);

	PreProc(inc_code.c_str());
	ExpectTokTypes(expected_toks);
}

TEST_F(PreProcIncludeTest, include2)
{
	std::string src = R"(#include <stdio.h>)";

	ExpectFileLoad("stdio.h", stdio_code);

	PreProc(src.c_str());
	ExpectTokTypes({ tok_void,
		tok_identifier,
		tok_l_paren,
		tok_r_paren,
		tok_semi_colon,
		tok_eof
		});
}

TEST_F(PreProcIncludeTest, include_leading_space)
{
	std::string src = R"(  #include <stdio.h>)";

	ExpectFileLoad("stdio.h", stdio_code);

	PreProc(src.c_str());
	ExpectTokTypes({ tok_void,
		tok_identifier,
		tok_l_paren,
		tok_r_paren,
		tok_semi_colon,
		tok_eof
		});
}

TEST_F(PreProcIncludeTest, include_line_cont1)
{
	std::string src = R"(#\
include <stdio.h>)";

	ExpectFileLoad("stdio.h", stdio_code);

	PreProc(src.c_str());
	ExpectTokTypes({ tok_void,
		tok_identifier,
		tok_l_paren,
		tok_r_paren,
		tok_semi_colon,
		tok_eof
		});
}

TEST_F(PreProcIncludeTest, include_line_cont2)
{
	std::string src = R"(#inc\
lude <stdio.h>)";

	ExpectFileLoad("stdio.h", stdio_code);

	PreProc(src.c_str());
	ExpectTokTypes({ tok_void,
		tok_identifier,
		tok_l_paren,
		tok_r_paren,
		tok_semi_colon,
		tok_eof
		});
}

TEST_F(PreProcIncludeTest, include_line_cont3)
{
	std::string src = R"(#include <stdio.\
h>)";

	ExpectFileLoad("stdio.h", stdio_code);

	PreProc(src.c_str());
	ExpectTokTypes({ tok_void,
		tok_identifier,
		tok_l_paren,
		tok_r_paren,
		tok_semi_colon,
		tok_eof
		});
}

TEST_F(PreProcIncludeTest, include_define1)
{
	ExpectFileLoad("stdio.h", stdio_code);

	const std::string inc_code = R"(
#define STDIO_H "stdio.h"
#include STDIO_H
void main() {})";

	PreProc(inc_code.c_str());
	ExpectTokTypes(expected_toks);
}

TEST_F(PreProcIncludeTest, include_define2)
{
	ExpectFileLoad("stdio.h", stdio_code);

	const std::string inc_code = R"(
#define STDIO_H <stdio.h>
#include STDIO_H
void main() {})";

	PreProc(inc_code.c_str());
	ExpectTokTypes(expected_toks);
}



TEST_F(PreProcIncludeTest, include_recursive)
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

TEST_F(PreProcIncludeTest, include_two_files)
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


#endif