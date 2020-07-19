#pragma once

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <stdint.h>
#include <vector>
#include <optional>

extern "C"
{
#include <libcomp/include/diag.h>
#include <libcomp/include/lexer.h>
#include <libcomp/include/parse.h>
#include <libcomp/include/validate.h>
}

using namespace ::testing;

class ValidationTest : public ::testing::Test
{
public:
	ValidationTest()
	{
		lex_init();

		std::function<void(token_t*, uint32_t, const char*)> fn =
			[this](token_t* tok, uint32_t err, const char* msg) {};

		diag_set_handler(&diag_cb, this);
		EXPECT_CALL(*this, on_diag(_, _, _)).Times(0);
	}

	~ValidationTest()
	{
		diag_set_handler(NULL, NULL);
	}

	void parse(const std::string& src)
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
			//free(tok);
			tok = next;
		}

		ast = parse_translation_unit(toks);
	}

	void validate()
	{
		valid_trans_unit_t* tl = tl_validate(ast);
		tl_destroy(tl);
	}

	static void diag_cb(token_t* tok, uint32_t err, const char* msg, void* data)
	{
		((ValidationTest*)data)->on_diag(tok, err, msg);
	}

	void ExpectError(const std::string& code, uint32_t err, testing::Cardinality times = Exactly(1))
	{
		EXPECT_CALL(*this, on_diag(_, err, _)).Times(times);
		parse(code);
		validate();
	}

	void ExpectNoError(const std::string& code)
	{
		EXPECT_CALL(*this, on_diag(_, _, _)).Times(Exactly(0));
		parse(code);
		validate();
	}

	void ExpectSyntaxErrors(const std::string& code)
	{
		EXPECT_CALL(*this, on_diag(_, ERR_SYNTAX, _)).Times(AtLeast(1));
		parse(code);
		EXPECT_EQ(ast, nullptr);
	}

	MOCK_METHOD3(on_diag, void(token_t* tok, uint32_t err, const char* msg));

	std::vector<token_t> tokens;
	ast_trans_unit_t* ast = nullptr;
};

