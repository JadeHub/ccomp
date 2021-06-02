#include "validation_fixture.h"

class PreProcIncludeTest : public LexTest
{
public:

	const std::string stdio_code = "void fn();";
	const std::string inc_code = R"(#include "stdio.h"
void main() {})";

	const std::string expected_code = R"(
void fn();
void main() {})";

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
	ExpectCode(expected_code);
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

TEST_F(PreProcIncludeTest, include_path1)
{
	ExpectFileLoad("inc/test/stdio.h", stdio_code);

	PreProc(R"(#include "inc/test/stdio.h")");
	ExpectCode(stdio_code);
}

TEST_F(PreProcIncludeTest, include_path2)
{
	ExpectFileLoad("inc/test/stdio.h", stdio_code);

	PreProc(R"(#include <inc/test/stdio.h>)");
	ExpectCode(stdio_code);
}

TEST_F(PreProcIncludeTest, include_path3)
{
	ExpectFileLoad("inc/test/stdio.h", stdio_code);

	PreProc(R"(#include <inc/test\stdio.h>)");
	ExpectCode(stdio_code);
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

	const std::string inc_code1 = R"(
#define STDIO_H "stdio.h"
#include STDIO_H
void main() {})";

	PreProc(inc_code1.c_str());
	ExpectCode(expected_code);
}

TEST_F(PreProcIncludeTest, include_define2)
{
	ExpectFileLoad("stdio.h", stdio_code);

	const std::string inc_code1 = R"(
#define STDIO_H <stdio.h>
#include STDIO_H
void main() {})";

	PreProc(inc_code1.c_str());
	ExpectCode(expected_code);
}

TEST_F(PreProcIncludeTest, include_recursive)
{
	std::string inc1 = R"(int x;)";
	std::string inc2 = R"(#include "inc1.h"
void fn();)";
	std::string src = R"(#include "inc2.h"
int j;
)";

	ExpectFileLoad("inc1.h", inc1);
	ExpectFileLoad("inc2.h", inc2);

	PreProc(src.c_str());
	ExpectCode(R"(int x;
void fn();
int j;
)");
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
	ExpectCode(R"(int x;
void fn();
)");
}

TEST_F(PreProcIncludeTest, pragma_once)
{
	std::string inc = R"(#pragma once
int i;)";

	//File load will be called twice so that the handler has the opportunity to search for a file
	ExpectFileLoad("inc1.h", inc, Exactly(2));

	std::string src = R"(#include "inc1.h"
#include "inc1.h"
)";

	PreProc(src.c_str());
	ExpectCode("int i;");
}

TEST_F(PreProcIncludeTest, __line__)
{
	std::string inc = R"(int i = 1;
int b = 1;
__LINE__

)";

	//File load will be called twice so that the handler has the opportunity to search for a file
	ExpectFileLoad("inc1.h", inc);

	std::string src = R"(#include "inc1.h"
__LINE__
)";

	PreProc(src.c_str());
	ExpectCode(R"(int i = 1;
int b = 1;
3
2)");
}
