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

TEST_F(EnumValidationTest, err_redefine_struct_as_enum)
{
	std::string code = R"(
	struct foo;

	enum foo
	{
		foo1,
		foo2
	};
	)";

	ExpectError(code, ERR_DUP_TYPE_DEF);
}

TEST_F(EnumValidationTest, err_redefine_as_struct)
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

TEST_F(EnumValidationTest, redefine_item_as_struct)
{
	//structs and enum members exist in different namespaces
	std::string code = R"(
	enum foo
	{
		dup
	};

	struct dup;
	)";

	ExpectNoError(code);
}

TEST_F(EnumValidationTest, err_redefine_item)
{
	std::string code = R"(	
	enum foo
	{
		dup
	};

	enum foo2
	{
		dup
	};
	)";

	ExpectError(code, ERR_DUP_SYMBOL);
}

TEST_F(EnumValidationTest, err_dup_item)
{
	std::string code = R"(	
	enum foo
	{
		dup,
		dup
	};
	)";

	ExpectError(code, ERR_DUP_SYMBOL);
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

TEST_F(EnumValidationTest, global_var_assign)
{
	std::string code = R"(	
	enum foo {foo1,	foo2};
	enum foo bar;

	void fn()
	{
		bar = foo1;
	}
	)";

	ExpectNoError(code);
}

TEST_F(EnumValidationTest, return_enum)
{
	std::string code = R"(	
	enum foo {foo1,	foo2};
	
	enum foo fn()
	{
		return foo1;
	}
	)";

	ExpectNoError(code);
}

TEST_F(EnumValidationTest, enum_member)
{
	std::string code = R"(	
	enum foo {foo1,	foo2};
	struct A { enum foo f;};
	
	enum foo fn()
	{
		struct A a;
		a.f = foo2;
		return a.f;
	}
	)";

	ExpectNoError(code);
}

TEST_F(EnumValidationTest, enum_param)
{
	std::string code = R"(	
	typedef enum {foo1,	foo2} foo ;

	int bar(foo f)
	{	
		return f;
	}	

	)";

	ExpectNoError(code);
}

