#include "validation_fixture.h"

class LocalVarsValidationTest : public CompilerTest {};

TEST_F(LocalVarsValidationTest, err_dup_var)
{
	std::string code = R"(
	void main()
	{
		int foo;
		int foo;
	}	
	)";

	ExpectCompilerError(code, ERR_DUP_VAR);
}

TEST_F(LocalVarsValidationTest, dup_new_scope)
{
	std::string code = R"(
	int main()
	{
		int foo;
		{
			int foo;
		}
		{
			return foo;
		}
	}	
	)";

	ExpectNoError(code);
}

TEST_F(LocalVarsValidationTest, err_undeclared_var)
{
	std::string code = R"(
	void main()
	{
		a = 1 + 2;
	}	
	)";

	ExpectCompilerError(code, ERR_UNKNOWN_IDENTIFIER);
}

TEST_F(LocalVarsValidationTest, err_incorrect_type_assign)
{
	std::string code = R"(
	struct A{int a;};
	void main()
	{
		int i;
		struct A a;
		i = a;
	}	
	)";

	ExpectCompilerError(code, ERR_INCOMPATIBLE_TYPE);
}

TEST_F(LocalVarsValidationTest, err_incorrect_type_initialisation)
{
	std::string code = R"(
	struct A{int a;};

	void main()
	{
		struct A a;
		int i = a;
	}	
	)";

	ExpectCompilerError(code, ERR_INCOMPATIBLE_TYPE);
}

TEST_F(LocalVarsValidationTest, char_variable)
{
	std::string code = R"(
	
	int main()
	{
		char c;
		c = 'j';
		return c;
	}	
	)";

	ExpectNoError(code);
}

