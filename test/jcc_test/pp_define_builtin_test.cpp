#include "validation_fixture.h"

class PreProcDefineBuiltInTest : public LexPreProcTest {};

TEST_F(PreProcDefineBuiltInTest, __file__)
{
	std::string path = "test/stdio.h";
	std::string inc_code = "__FILE__";
	ExpectFileLoad(path, inc_code);

	std::string src = R"(#include <test/stdio.h>)";

	PreProc(src);
	ExpectTokTypes({ tok_string_literal, tok_eof });
	EXPECT_THAT(GetToken(0)->data.str, EndsWith("/test/stdio.h"));
}

TEST_F(PreProcDefineBuiltInTest, __line__)
{
	std::string src = R"(__LINE__
A
__LINE__
A
__LINE__
A
__LINE__
A
__LINE__
A
__LINE__
A
__LINE__
A
__LINE__
)";

	PreProc(src);
	ExpectCode(R"(1
A
3
A
5
A
7
A
9
A
11
A
13
A
15
)");
}