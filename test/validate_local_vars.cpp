#include "validation_fixture.h"

class LocalVarsValidationTest : public ValidationTest {};

TEST_F(LocalVarsValidationTest, err_dup_var)
{
	std::string code = R"(
	void main()
	{
		int foo;
		int foo;
	}	
	)";

	ExpectError(code, ERR_DUP_VAR);
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

	ExpectError(code, ERR_UNKNOWN_VAR);
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

	ExpectError(code, ERR_INCOMPATIBLE_TYPE);
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

