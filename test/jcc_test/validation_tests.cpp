#include "validation_fixture.h"

TEST_F(ValidationTest, fn_missing_r_brace)
{
	ExpectSyntaxErrors("int main() {return 7;");
}

TEST_F(ValidationTest, fn_missing_r_paren)
{
	ExpectError("int main( {return 7;}", ERR_UNKNOWN_TYPE);
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

TEST_F(ValidationTest, unary_missing_space)
{
	ExpectSyntaxErrors("int main() {return7}");
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

	ExpectNoError(code);
}

TEST_F(ValidationTest, unary_wrong_order)
{
	ExpectSyntaxErrors("int main() {return 4-;}");
}

TEST_F(ValidationTest, unary_missing_const)
{
	ExpectSyntaxErrors("int main() {return ~;}");
}

TEST_F(ValidationTest, unary_nested_missing_const)
{
	ExpectSyntaxErrors("int main() {return !~;}");
}

TEST_F(ValidationTest, unary_nested_missing_semi)
{
	ExpectSyntaxErrors("int main() {return !5}");
}

TEST_F(ValidationTest, binary_missing_semi)
{
	ExpectSyntaxErrors("int main() {return 2*2}");
}

TEST_F(ValidationTest, binary_missing_rhs)
{
	ExpectSyntaxErrors("int main() {return 2 + ;}");
}

TEST_F(ValidationTest, binary_missing_lhs)
{
	ExpectSyntaxErrors("int main() {return / 3;}");
}

TEST_F(ValidationTest, boolean_missing_rhs)
{
	ExpectSyntaxErrors("int main() {return 2 &&;}");
}

TEST_F(ValidationTest, boolean_missing_lhs)
{
	ExpectSyntaxErrors("int main() {return <= 3;}");
}