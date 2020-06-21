#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <vector>

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

	void parse(const char* src)
	{
		source_range_t sr;
		sr.ptr = src;
		sr.end = sr.ptr + strlen(src);
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
		validate_tl(ast);
	}

	static void diag_cb(token_t* tok, uint32_t err, const char* msg, void* data)
	{
		((ValidationTest*)data)->on_diag(tok, err, msg);
	}

	void ExpectError(uint32_t err)
	{
		EXPECT_CALL(*this, on_diag(_, err, _));
	}

	MOCK_METHOD3(on_diag, void(token_t* tok, uint32_t err, const char* msg));

	std::vector<token_t> tokens;
	ast_trans_unit_t* ast = nullptr;
};

TEST_F(ValidationTest, global_var_shadows_fn)
{
	std::string code = R"(
	int foo(int a);
	int foo = 5;	)";

	ExpectError(ERR_DUP_SYMBOL);

	parse(code.c_str());
	validate();
}

TEST_F(ValidationTest, global_var_dup_definition)
{
	std::string code = R"(
	int foo = 4;
	int foo = 5;	)";

	ExpectError(ERR_DUP_VAR);

	parse(code.c_str());
	validate();
}

TEST_F(ValidationTest, global_var_invalid_init)
{
	std::string code = R"(
	int fooA = 4;
	int foo = fooA;	)";

	ExpectError(ERR_INVALID_INIT);

	parse(code.c_str());
	validate();
}

TEST_F(ValidationTest, global_fn_shadows_var)
{
	std::string code = R"(
	int foo = 5;	
	int foo(int a);)";

	ExpectError(ERR_DUP_SYMBOL);

	parse(code.c_str());
	validate();
}

TEST_F(ValidationTest, fn_decl_in_fn_definition)
{
	std::string code = R"(
	int main()
	{
		int foo();
		return 2;
	}
	)";

	parse(code.c_str());
	validate();
}

TEST_F(ValidationTest, foo)
{
	std::string code = R"(

	struct B {
		int a;
		int b;
	} v;

	)";
	parse(code.c_str());
}
