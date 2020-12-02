#include "validation_fixture.h"

class PreProcDefineBuiltInTest : public LexPreProcTest {};

/*TEST_F(PreProcDefineBuiltInTest, __file__)
{
	std::string src = R"(__FILE__)";

	PreProc(src);
	PrintTokens();
}*/

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