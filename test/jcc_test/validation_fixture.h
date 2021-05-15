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
#include <libcomp/include/sema_internal.h>
#include <libcomp/include/pp.h>
#include <libcomp/include/pp_internal.h>
#include <libcomp/include/abi.h>
#include <libcomp/include/parse_internal.h>
#include <libcomp/include/std_types.h>
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

	CompilerTest()
	{
		lex_init();
		sema_observer_t observer{ nullptr };
		sema_init(observer);
	}

	void SetSource(const std::string& src)
	{
		mSrc = src;
	}

	bool Lex()
	{
		source_range_t sr;
		sr.ptr = mSrc.c_str();
		sr.end = sr.ptr + mSrc.length();

		mTokens = lex_source(&sr).start;
		return mTokens != nullptr;
	}

	bool Parse()
	{
		parse_init(mTokens);
		mAst = parse_translation_unit();
		return mAst != nullptr;
	}

	bool LexAndParse(const std::string& src)
	{
		SetSource(src);
		if (!Lex())
			return false;

		if (!Parse())
			return false;
		return true;
	}

	bool Analyse()
	{
		mTL = sema_analyse(mAst);
		return mTL != nullptr;
	}

	void ExpectCompilerError(const std::string& code, uint32_t err, testing::Cardinality times = Exactly(1))
	{
		TestWithErrorHandling::ExpectError(err, times);

		if (LexAndParse(code))
			Analyse();
	}

	void ExpectNoError(const std::string& code)
	{
		TestWithErrorHandling::ExpectNoError();
		ASSERT_TRUE(LexAndParse(code));
		ASSERT_TRUE(Analyse());
	}

	std::string mSrc;
	token_t* mTokens = nullptr;
	ast_trans_unit_t* mAst = nullptr;
	valid_trans_unit_t* mTL = nullptr;
};

class LexTest : public TestWithErrorHandling
{
public:

	LexTest()
	{
		src_init(&load_file, this);
		lex_init();
		pre_proc_init();
	}

	~LexTest()
	{
		pre_proc_deinit();
		src_deinit();
	}

	void Lex(const std::string& src, const std::string path = "test.c")
	{
		source_range_t sr;
		sr.ptr = src.c_str();
		sr.end = sr.ptr + src.length();

		src_register_range(sr, strdup(path.c_str()));

		tokens = lex_source(&sr);
	}

	token_range_t tokens = { NULL, NULL };

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
		EXPECT_EQ(int_val_as_uint32(&t->data.int_val), (uint32_t)val);
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

	static source_range_t load_file(const char* dir, const char* file, void* data)
	{
		dir;
		LexTest* This = (LexTest*)data;
		return This->on_load_file(file);
	}

	void PreProc(const std::string& src, const std::string path = "test.c")
	{
		Lex(src, path);
		if (!tok_range_empty(&tokens))
			tokens = pre_proc_file(".", &tokens);
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
