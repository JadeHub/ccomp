#include "validation_fixture.h"

class PreProcDefineTest : public LexPreProcTest {};

TEST_F(PreProcDefineTest, define)
{
	std::string src = R"(#define TEST int i;
TEST)";

	PreProc(src.c_str());
	ExpectTokTypes({ tok_int,
		tok_identifier,
		tok_semi_colon,
		tok_eof });
}

TEST_F(PreProcDefineTest, err_dup_define)
{
	std::string src = R"(
#define TEST
#define TEST
)";

	ExpectError(ERR_SYNTAX);
	PreProc(src.c_str());
}
