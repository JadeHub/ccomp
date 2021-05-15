#include "validation_fixture.h"

class ExprValidationTest : public CompilerTest
{};

TEST_F(ExprValidationTest, unary_missing_val)
{
	ExpectCompilerError("int main() {return !;}", ERR_SYNTAX, ::testing::Cardinality(AtLeast(1)));
}

TEST_F(ExprValidationTest, unary_nested_missing_val)
{
	ExpectCompilerError("int main() {return !~;}", ERR_SYNTAX, ::testing::Cardinality(AtLeast(1)));
}

TEST_F(ExprValidationTest, unary_missing_semi)
{
	ExpectCompilerError("int main() {return !7}", ERR_SYNTAX, ::testing::Cardinality(AtLeast(1)));
}

TEST_F(ExprValidationTest, unary_missing_space)
{
	ExpectCompilerError("int main() {return7}", ERR_SYNTAX, ::testing::Cardinality(AtLeast(1)));
}

TEST_F(ExprValidationTest, unary_wrong_order)
{
	ExpectCompilerError("int main() {return 4-;}", ERR_SYNTAX, ::testing::Cardinality(AtLeast(1)));
}

TEST_F(ExprValidationTest, unary_missing_const)
{
	ExpectCompilerError("int main() {return ~;}", ERR_SYNTAX, ::testing::Cardinality(AtLeast(1)));
}

TEST_F(ExprValidationTest, unary_nested_missing_const)
{
	ExpectCompilerError("int main() {return !~;}", ERR_SYNTAX, ::testing::Cardinality(AtLeast(1)));
}

TEST_F(ExprValidationTest, unary_nested_missing_semi)
{
	ExpectCompilerError("int main() {return !5}", ERR_SYNTAX, ::testing::Cardinality(AtLeast(1)));
}

TEST_F(ExprValidationTest, binary_missing_semi)
{
	ExpectCompilerError("int main() {return 2*2}", ERR_SYNTAX, ::testing::Cardinality(AtLeast(1)));
}

TEST_F(ExprValidationTest, binary_missing_rhs)
{
	ExpectCompilerError("int main() {return 2 + ;}", ERR_SYNTAX, ::testing::Cardinality(AtLeast(1)));
}

TEST_F(ExprValidationTest, binary_missing_lhs)
{
	ExpectCompilerError("int main() {return / 3;}", ERR_SYNTAX, ::testing::Cardinality(AtLeast(1)));
}

TEST_F(ExprValidationTest, boolean_missing_rhs)
{
	ExpectCompilerError("int main() {return 2 &&;}", ERR_SYNTAX, ::testing::Cardinality(AtLeast(1)));
}

TEST_F(ExprValidationTest, boolean_missing_lhs)
{
	ExpectCompilerError("int main() {return <= 3;}", ERR_SYNTAX, ::testing::Cardinality(AtLeast(1)));
}

TEST_F(ExprValidationTest, conditional_return)
{
	std::string code = R"(
	char* foo()
	{
		char* i, *j;
		int b = 1;
		return b ? i : j;
	}
	)";

	ExpectNoError(code);
}