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
#include <libcomp/include/sema.h>
#include <libcomp/include/pp.h>
#include <libcomp/include/pp_internal.h>
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

class CompilerTest : public TestWithErrorHandling
{
public:

	void SetSource(const std::string& src)
	{
		mSrc = src;
	}

	void Lex()
	{
		source_range_t sr;
		sr.ptr = mSrc.c_str();
		sr.end = sr.ptr + mSrc.length();

		mTokens = lex_source(&sr).start;
	}

	void Parse()
	{
		mAst = parse_translation_unit(mTokens);
	}

	void Validate()
	{
		mTL = sem_analyse(mAst);
	}

private:

	std::string mSrc;
	token_t* mTokens = nullptr;
	ast_trans_unit_t* mAst = nullptr;
	valid_trans_unit_t* mTL = nullptr;
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
		token_t* toks = lex_source(&sr).start;
				
		ast = parse_translation_unit(toks);
	}

	void validate()
	{
		if (ast)
		{
			tl = sem_analyse(ast);
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

	ast_function_decl_t* AstFunction(const std::string& name)
	{
		tl_decl_t* fn = tl->fn_decls;

		while (fn)
		{
			if (name == fn->decl->data.func.name)
			{
				return &fn->decl->data.func;
			}
			fn = fn->next;
		}
		//FAIL() << "failed to find function " << name;
		//return NULL;
		//ASSERT_EQ(1, 2);
	}

	//std::vector<token_t*> tokens;
	ast_trans_unit_t* ast = nullptr;
	valid_trans_unit_t* tl = nullptr;
};

class LexTest : public TestWithErrorHandling
{
public:

	LexTest()
	{
		lex_init();
	}

	~LexTest()
	{
	}

	void Lex(const std::string& src)
	{
		source_range_t sr;
		sr.ptr = src.c_str();
		sr.end = sr.ptr + src.length();

		tokens = lex_source(&sr);
	}

	token_range_t tokens = { NULL, NULL };
	//token_t* tokens = nullptr;

	token_t* GetToken(uint32_t idx)
	{
		token_t* tok = tokens.start;
		for (uint32_t i = 0; i < idx; i++)
		{
			EXPECT_NE(nullptr, tok);
			tok = tok->next;
		}
		return tok;
	}

	template <typename T>
	void ExpectIntLiteral(token_t* t, T val)
	{
		EXPECT_EQ(t->kind, tok_num_literal);
		EXPECT_EQ(t->data.integer, (uint32_t)val);
	}

	void ExpectStringLiteral(token_t* t, const char* expected)
	{
		EXPECT_STREQ(t->data.str, expected);
	}

	void ExpectTokTypes(const std::vector<tok_kind>& kinds)
	{
		token_t* tok = tokens.start;

		for (auto i = 0; i < kinds.size(); i++)
		{
			ASSERT_NE(nullptr, tok);
			EXPECT_EQ(kinds[i], tok->kind);
			tok = tok->next;
		}
	}

	void PrintTokens()
	{
		token_t* tok = tokens.start;

		while (tok != tokens.end)
		{
			tok_printf(tok);
			tok = tok->next;
		}
	}

	void ExpectCode(const std::string& code)
	{
		source_range_t sr;
		sr.ptr = code.c_str();
		sr.end = sr.ptr + code.length();

		token_range_t expected = lex_source(&sr);
		
		EXPECT_TRUE(tok_range_equals(&tokens, &expected));
	}
};

class LexPreProcTest : public LexTest
{
public:

	LexPreProcTest()
	{
		src_init("./", &load_file, this);
		pre_proc_init();
	}

	~LexPreProcTest()
	{
		pre_proc_deinit();
		src_deinit();
	}

	static source_range_t load_file(const char* dir, const char* file, void* data)
	{
		LexPreProcTest* This = (LexPreProcTest*)data;
		return This->on_load_file(file);
	}

	void PreProc(const std::string& src)
	{
		Lex(src);
		if(!tok_range_empty(&tokens))
			tokens = pre_proc_file(&tokens);
	}

	MOCK_METHOD1(on_load_file, source_range_t(const std::string&));

	void ExpectFileLoad(const std::string& path, const std::string& code, testing::Cardinality times = Exactly(1))
	{
		source_range_t sr;
		sr.ptr = code.c_str();
		sr.end = sr.ptr + code.size() + 1;
		EXPECT_CALL(*this, on_load_file(path)).Times(times).WillRepeatedly(Return(sr));
	}
};