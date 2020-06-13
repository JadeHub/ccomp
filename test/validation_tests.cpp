#include <gtest/gtest.h>
#include <gmock/gmock.h>

extern "C"
{
#include <libcomp/include/diag.h>
#include <libcomp/include/lexer.h>
#include <libcomp/include/parse.h>
#include <libcomp/include/validate.h>
}

class ValidationTest : public ::testing::Test
{
public:
	ValidationTest()
	{
		lex_init();

		std::function<void(token_t*, uint32_t, const char*)> fn = 
			[this](token_t* tok, uint32_t err, const char* msg) {};

		diag_set_handler(&diag_cb, this);
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
		toks = lex_source(&sr);
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

	MOCK_METHOD3(on_diag, void(token_t* tok, uint32_t err, const char* msg));

	token_t* toks = nullptr;
	ast_trans_unit_t* ast = nullptr;
};


TEST_F(ValidationTest, foo)
{
	std::string code = R"(
int foo(int a){
    return 3 + a;
}

int main(){
    return foo();
}

)";

	parse(code.c_str());
	validate();
}