#include "validation_fixture.h"

class LexerTest : public LexTest {};

TEST_F(LexerTest, ln_cnt)
{
	std::string code = R"(line 1;
line 2;
line 3;
line 4;
)";
	/*	Line		Tokens
		1			0 - 2
		2			3 - 5
		3			6 - 8
		4			9 - 11
	*/

	Lex(code);
	EXPECT_EQ(1, src_file_position(GetToken(0)->loc).line);
	EXPECT_EQ(1, src_file_position(GetToken(1)->loc).line);
	EXPECT_EQ(1, src_file_position(GetToken(2)->loc).line);

	EXPECT_EQ(2, src_file_position(GetToken(3)->loc).line);
	EXPECT_EQ(2, src_file_position(GetToken(4)->loc).line);
	EXPECT_EQ(2, src_file_position(GetToken(5)->loc).line);

	EXPECT_EQ(3, src_file_position(GetToken(6)->loc).line);
	EXPECT_EQ(3, src_file_position(GetToken(7)->loc).line);
	EXPECT_EQ(3, src_file_position(GetToken(8)->loc).line);

	EXPECT_EQ(4, src_file_position(GetToken(9)->loc).line);
	EXPECT_EQ(4, src_file_position(GetToken(10)->loc).line);
	EXPECT_EQ(4, src_file_position(GetToken(11)->loc).line);

	
}

TEST_F(LexerTest, foo)
{
	Lex(R"(int foo = 0;)");
}

TEST_F(LexerTest, eof)
{
	std::string code = R"(int foo(int a);
int foo = 5;)";

	Lex(code);
	ExpectCode(R"(
int foo(int a);
int foo = 5;)");
}

TEST_F(LexerTest, comment_slashslash)
{
	std::string code = R"(
int foo //(int a);
int foo2 = 5;)";

	Lex(code);
	ExpectCode(R"(
int foo
int foo2 = 5;)");
}

TEST_F(LexerTest, comment_mid_line)
{
	std::string code = R"(
int foo /* abc */;
int foo2 = 5;)";

	Lex(code);
	ExpectCode(R"(
int foo ;
int foo2 = 5;)");
}

TEST_F(LexerTest, comment_multiline)
{
	std::string code = R"(
int foo /* abc ;
int foo2 = 5;*/)";

	Lex(code);
	ExpectCode(R"(
int foo )");
}

TEST_F(LexerTest, comment_entire_file)
{
	std::string code = R"(
/*int foo abc ;
int foo2 = 5;*/)";

	Lex(code);
	ExpectCode(" ");
}

TEST_F(LexerTest, err_comment_unterminated)
{
	std::string code = R"(
int foo /* abc ;
int foo2 = 5;)";

	ExpectError(ERR_SYNTAX);

	Lex(code);
}

TEST_F(LexerTest, hex_const_1)
{
	std::string code = R"(int x = 0xFF;)";

	Lex(code);
	ExpectIntLiteral(GetToken(3), 0xFF);
}

TEST_F(LexerTest, hex_const_2)
{
	std::string code = R"(int x = 0xFf;)";

	Lex(code);
	ExpectIntLiteral(GetToken(3), 0xFF);
}

TEST_F(LexerTest, hex_const_3)
{
	std::string code = R"(int x = 0x00000;)";

	Lex(code);
	ExpectIntLiteral(GetToken(3), 0);
}

TEST_F(LexerTest, hex_const_4)
{
	std::string code = R"(int x = 0x00000acb;)";

	Lex(code);
	ExpectIntLiteral(GetToken(3), 0xACB);
}

TEST_F(LexerTest, hex_const_5)
{
	std::string code = R"(int x = 0x00000acb; int main())";

	Lex(code);
	ExpectIntLiteral(GetToken(3), 0xACB);
	EXPECT_EQ(GetToken(4)->kind, tok_semi_colon);
	EXPECT_EQ(GetToken(5)->kind, tok_int);
	EXPECT_EQ(GetToken(6)->kind, tok_identifier);
}

TEST_F(LexerTest, err_char_const_too_large)
{
	std::string code = R"(int x = '\xFF1';)";

	ExpectError(ERR_SYNTAX);
	Lex(code);
}

TEST_F(LexerTest, oct_const_1)
{
	std::string code = R"(int x = 052;)";

	Lex(code);
	ExpectIntLiteral(GetToken(3), 42);
}

TEST_F(LexerTest, const_const_1)
{
	std::string code = R"(int x = 'a';)";

	Lex(code);
	ExpectIntLiteral(GetToken(3), 'a');
}

TEST_F(LexerTest, char_const_esc_1)
{
	std::string code = R"(int x = '\t';)";

	Lex(code);
	ExpectIntLiteral(GetToken(3), '\t');
}

TEST_F(LexerTest, char_const_null)
{
	std::string code = R"(int x = '\0';)";

	Lex(code);
	ExpectIntLiteral(GetToken(3), 0);
}

TEST_F(LexerTest, char_const_esc_hex)
{
	std::string code = R"(int x = '\xFF';)";

	Lex(code);
	ExpectIntLiteral(GetToken(3), 0xFF);
}

TEST_F(LexerTest, char_const_esc_hex_2)
{
	std::string code = R"(int x = '\x0F';)";

	Lex(code);
	ExpectIntLiteral(GetToken(3), 0x0F);
}

TEST_F(LexerTest, char_const_esc_oct_1)
{
	std::string code = R"(int x = '\052';)";

	Lex(code);
	ExpectIntLiteral(GetToken(3), 42);
}

TEST_F(LexerTest, err_char_const_bas_esc)
{
	std::string code = R"(int x = '\q';)";

	ExpectError(ERR_SYNTAX);
	Lex(code);
}

TEST_F(LexerTest, err_char_const_empty)
{
	std::string code = R"(int x = '';)";

	ExpectError(ERR_SYNTAX);
	Lex(code);
}

TEST_F(LexerTest, err_char_const_too_long)
{
	std::string code = R"(int x = '\nr';)";

	ExpectError(ERR_SYNTAX);
	Lex(code);
}

TEST_F(LexerTest, err_char_const_unterminated)
{
	std::string code = R"(int x = 'a;)";

	ExpectError(ERR_SYNTAX);
	Lex(code);
}

TEST_F(LexerTest, err_char_const_unterminated_2)
{
	std::string code = R"(int x = 'a
						';)";

	ExpectError(ERR_SYNTAX);
	Lex(code);
}

TEST_F(LexerTest, string_const)
{
	std::string code = R"(char* c = "abc";)";

	Lex(code);
	ExpectStringLiteral(GetToken(4), "abc");
}

TEST_F(LexerTest, empty_string_const)
{
	std::string code = R"(char* c = "";)";

	Lex(code);
	ExpectStringLiteral(GetToken(4), "");
}

TEST_F(LexerTest, err_string_const_unterminated)
{
	std::string code = R"(char* c = "abc;)";

	ExpectError(ERR_SYNTAX);
	Lex(code);
}

TEST_F(LexerTest, err_string_const_unterminated_2)
{
	std::string code = R"(char* c = "abc;
							")";

	ExpectError(ERR_SYNTAX);
	Lex(code);
}

TEST_F(LexerTest, string_constant_1)
{
	std::string code = R"("hello \"bob\"")";

	Lex(code);
	ExpectStringLiteral(GetToken(0), "hello \"bob\"");
}

TEST_F(LexerTest, tok_spelling_len)
{
	Lex(R"(abc)");

	ExpectTokTypes({ tok_identifier, tok_eof });
	EXPECT_EQ(3, tok_spelling_len(GetToken(0)));
}
