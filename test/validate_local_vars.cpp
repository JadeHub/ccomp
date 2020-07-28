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
	void main()
	{
		int foo;

		{
			int foo;
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





/*TEST_F(LocalVarsValidationTest, err_dup_definition)
{
	std::string code = R"(
	int foo = 4;
	int foo = 5;	)";

	ExpectError(code, ERR_DUP_SYMBOL);
}

TEST_F(LocalVarsValidationTest, err_change_type)
{
	std::string code = R"(
	int foo = 4;
	void foo;	)";

	ExpectError(code, ERR_DUP_SYMBOL);
}

TEST_F(LocalVarsValidationTest, err_change_type2)
{
	std::string code = R"(
	int foo = 4;
	struct B{int b;} foo;	)";

	ExpectError(code, ERR_DUP_SYMBOL);
}

TEST_F(LocalVarsValidationTest, err_struct_redefined)
{
	std::string code = R"(
	struct B{int b;} foo;
	struct B{int b;} foo1;	)";

	ExpectError(code, ERR_DUP_TYPE_DEF);
}

TEST_F(LocalVarsValidationTest, err_struct_incomplete)
{
	std::string code = R"(
	struct B foo;
	)";

	ExpectError(code, ERR_TYPE_INCOMPLETE);
}

TEST_F(LocalVarsValidationTest, struct_referenced)
{
	std::string code = R"(
	struct B{int b;} foo;
	struct B foo1;
	)";

	ExpectNoError(code);
}

*/

