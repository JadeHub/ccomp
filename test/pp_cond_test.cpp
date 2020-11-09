#if 0

#include "validation_fixture.h"

class PreProcCondTest : public LexPreProcTest {};

TEST_F(PreProcCondTest, pp_null)
{
	std::string src = R"(
#
)";

	PreProc(src.c_str());
	ExpectTokTypes({ tok_eof });
}

TEST_F(PreProcCondTest, if_true)
{
	std::string src = R"(
#if 1
int i;
#endif)";

	PreProc(src.c_str());
	ExpectTokTypes({ tok_int,
		tok_identifier,
		tok_semi_colon,
		tok_eof });
}

TEST_F(PreProcCondTest, if_false)
{
	std::string src = R"(
#if 0
int i;
#endif)";

	PreProc(src.c_str());
	ExpectTokTypes({ tok_eof });
}

TEST_F(PreProcCondTest, if_defined_true)
{
	std::string src = R"(
#define ABC 1
#if defined(ABC)
int i;
#endif)";

	PreProc(src.c_str());
	ExpectTokTypes({ tok_int,
		tok_identifier,
		tok_semi_colon,
		tok_eof });
}

TEST_F(PreProcCondTest, if_defined_else_true)
{
	std::string src = R"(
#define ABC
#if !defined(ABC)
char c;
#else
int i;
#endif)";

	PreProc(src.c_str());
	ExpectTokTypes({ tok_int,
		tok_identifier,
		tok_semi_colon,
		tok_eof });
}

TEST_F(PreProcCondTest, if_defined_false)
{
	std::string src = R"(
#if defined(ABC)
int i;
#endif)";

	PreProc(src.c_str());
	ExpectTokTypes({ tok_eof });
}

TEST_F(PreProcCondTest, if_not_defined)
{
	std::string src = R"(
#if !defined(ABC)
int i;
#endif)";

	PreProc(src.c_str());
	ExpectTokTypes({ tok_int,
		tok_identifier,
		tok_semi_colon,
		tok_eof });
}

TEST_F(PreProcCondTest, if_elif)
{
	std::string src = R"(
#define ABC
#if !defined(ABC)
char c;
#elif defined(ABC)
int i;
#endif)";

	PreProc(src.c_str());
	ExpectTokTypes({ tok_int,
		tok_identifier,
		tok_semi_colon,
		tok_eof });
}

TEST_F(PreProcCondTest, if_else_if)
{
	std::string src = R"(
#define ABC
#if !defined(ABC)
char c;
#else if defined(ABC)
int i;
#endif)";

	PreProc(src.c_str());
	ExpectTokTypes({ tok_int,
		tok_identifier,
		tok_semi_colon,
		tok_eof });
}

TEST_F(PreProcCondTest, ifdef_true)
{
	std::string src = R"(
#define ABC 1
#ifdef ABC
int i;
#endif)";

	PreProc(src.c_str());
	ExpectTokTypes({ tok_int,
		tok_identifier,
		tok_semi_colon,
		tok_eof });
}

TEST_F(PreProcCondTest, ifdef_true_else)
{
	std::string src = R"(
#define ABC 1
#ifdef ABC
int i;
#else
char c;
#endif)";

	PreProc(src.c_str());
	ExpectTokTypes({ tok_int,
		tok_identifier,
		tok_semi_colon,
		tok_eof });
}

TEST_F(PreProcCondTest, ifdef_false_else)
{
	std::string src = R"(
#ifdef ABC
int i;
#else
char c;
#endif)";

	PreProc(src.c_str());
	ExpectTokTypes({ tok_char,
		tok_identifier,
		tok_semi_colon,
		tok_eof });
}

TEST_F(PreProcCondTest, ifdef_false)
{
	std::string src = R"(
#ifdef ABC
int i;
#endif)";

	PreProc(src.c_str());
	ExpectTokTypes({ tok_eof });
}

TEST_F(PreProcCondTest, ifdef_define_no_toks)
{
	//ABC is defined but is empty
	std::string src = R"(
#define ABC
#ifdef ABC
int i;
#endif)";

	PreProc(src.c_str());
	ExpectTokTypes({ tok_int,
		tok_identifier,
		tok_semi_colon,
		tok_eof });
}

TEST_F(PreProcCondTest, ifndef_false)
{
	std::string src = R"(
#define ABC 1
#ifndef ABC
int i;
#endif)";

	PreProc(src.c_str());
	ExpectTokTypes({ 
		tok_eof });
}

TEST_F(PreProcCondTest, ifndef_true)
{
	std::string src = R"(
#ifndef ABC
int i;
#endif)";

	PreProc(src.c_str());
	ExpectTokTypes({ tok_int,
		tok_identifier,
		tok_semi_colon,
		tok_eof });
}

TEST_F(PreProcCondTest, undef)
{
	std::string src = R"(
#define ABC 1
#undef ABC
#ifndef ABC
int i;
#endif)";

	PreProc(src.c_str());
	ExpectTokTypes({ tok_int,
		tok_identifier,
		tok_semi_colon,
		tok_eof });
}

#endif