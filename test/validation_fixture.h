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

struct TestWithErrorHandling : public ::testing::Test 
{
	TestWithErrorHandling()
	{
		diag_set_handler(&diag_cb, this);
		EXPECT_CALL(*this, on_diag(_, _, _)).Times(0);
	}

	virtual ~TestWithErrorHandling()
	{
		diag_set_handler(NULL, NULL);
	}

	void ExpectError(uint32_t err, testing::Cardinality times = Exactly(1))
	{
		EXPECT_CALL(*this, on_diag(_, err, _)).Times(times);
	}

	void ExpectNoError()
	{
		EXPECT_CALL(*this, on_diag(_, _, _)).Times(Exactly(0));
	}

	static void diag_cb(token_t* tok, uint32_t err, const char* msg, void* data)
	{
		((TestWithErrorHandling*)data)->on_diag(tok, err, msg);
	}

	MOCK_METHOD3(on_diag, void(token_t* tok, uint32_t err, const char* msg));
};

class ValidationTest : public TestWithErrorHandling
{
public:
	ValidationTest()
	{
		lex_init();
	}

	virtual ~ValidationTest()
	{
		tl_destroy(tl);
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
		if (ast)
		{
			tl = tl_validate(ast);			
		}
	}

	static void diag_cb(token_t* tok, uint32_t err, const char* msg, void* data)
	{
		((ValidationTest*)data)->on_diag(tok, err, msg);
	}

	void ExpectError(const std::string& code, uint32_t err, testing::Cardinality times = Exactly(1))
	{
		TestWithErrorHandling::ExpectError(err, times);
		parse(code);
		if(ast)
			validate();
	}

	void ExpectNoError(const std::string& code)
	{
		TestWithErrorHandling::ExpectNoError();
		parse(code);
		validate();
	}

	void ExpectSyntaxErrors(const std::string& code)
	{
		TestWithErrorHandling::ExpectError(ERR_SYNTAX, ::testing::Cardinality(AtLeast(1)));
		parse(code);
		EXPECT_EQ(ast, nullptr);
	}

	std::vector<token_t> tokens;
	ast_trans_unit_t* ast = nullptr;
	valid_trans_unit_t* tl = nullptr;
};

