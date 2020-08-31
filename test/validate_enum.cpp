#include "validation_fixture.h"

class EnumValidationTest : public ValidationTest {};

/*
//todo fails with missing ;
struct B {struct A a; int b};

*/

TEST_F(EnumValidationTest, define)
{
	std::string code = R"(	
	enum foo
	{
		foo1,
		foo2
	};
	)";

	ExpectNoError(code);
}

TEST_F(EnumValidationTest, define_nested)
{
	std::string code = R"(	
	enum foo
	{
		foo1,
		foo2
	};

	void fn()
	{
		enum foo {a, b};	
		//enum foo f = a;
	}
	)";

	ExpectNoError(code);
}

TEST_F(EnumValidationTest, declare_define)
{
	std::string code = R"(	
	enum foo;

	enum foo
	{
		foo1,
		foo2
	};
	)";

	ExpectNoError(code);
}

TEST_F(EnumValidationTest, err_redefine)
{
	std::string code = R"(	
	enum foo
	{
		foo1,
		foo2
	};

	enum foo {blah};
	)";

	ExpectError(code, ERR_DUP_TYPE_DEF);
}

TEST_F(EnumValidationTest, err_redefine_struct)
{
	std::string code = R"(	
	enum foo
	{
		foo1,
		foo2
	};

	struct foo;
	)";

	ExpectError(code, ERR_DUP_TYPE_DEF);
}

TEST_F(EnumValidationTest, var)
{
	std::string code = R"(	
	enum foo {foo1,	foo2};
	void fn()
	{
		enum foo f;
	}
	)";

	ExpectNoError(code);
}

TEST_F(EnumValidationTest, var_assign)
{
	std::string code = R"(	
	enum foo {foo1,	foo2};
	void fn()
	{
		enum foo f;
		f = foo1;
	}
	)";

	ExpectNoError(code);
}

TEST_F(EnumValidationTest, var_assign_same_name)
{
	std::string code = R"(	
	enum foo {foo1,	foo2};
	void fn()
	{
		enum foo foo;
		foo = foo1;
	}
	)";

	ExpectNoError(code);
}

