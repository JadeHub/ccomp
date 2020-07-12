#include "validation_fixture.h"

TEST_F(ValidationTest, fn_missing_r_brace)
{
	ExpectSyntaxErrors("int main() {return 7;");
}

TEST_F(ValidationTest, fn_missing_r_paren)
{
	ExpectSyntaxErrors("int main( {return 7;}");
}

TEST_F(ValidationTest, fn_missing_ret_val)
{
	ExpectSyntaxErrors("int main() {return;}");
}

TEST_F(ValidationTest, fn_missing_semi)
{
	ExpectSyntaxErrors("int main() {return 7}");
}

TEST_F(ValidationTest, fn_ret_misspelt)
{
	ExpectSyntaxErrors("int main() {retrn 7}");
}

TEST_F(ValidationTest, unary_missing_val)
{
	ExpectSyntaxErrors("int main() {return !;}");
}

TEST_F(ValidationTest, unary_nested_missing_val)
{
	ExpectSyntaxErrors("int main() {return !~;}");
}

TEST_F(ValidationTest, unary_missing_semi)
{
	ExpectSyntaxErrors("int main() {return !7}");
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

	parse(code);
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
	parse(code);
}
