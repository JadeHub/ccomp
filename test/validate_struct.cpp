#include "validation_fixture.h"

class StructValidationTest : public ValidationTest {};

/*
//todo fails with missing ;
struct B {struct A a; int b};

*/

TEST_F(StructValidationTest, return_sub_member)
{
	std::string code = R"(
	struct B {int a; int b;};
	struct A {struct B ab; int i;};

	int main()
	{
		struct A a;

		a.ab.a = 1;
		a.ab.b = 10;
		a.i = 100;

		return a.ab.b;
	}
	)";

	ExpectNoError(code);
}

TEST_F(StructValidationTest, decl_define)
{
	std::string code = R"(
	struct A;
	struct A {int i;};

	int main()
	{
		struct A a;

		a.i = 100;

		return a.i;
	}
	)";

	ExpectNoError(code);
}

TEST_F(StructValidationTest, redefine_new_scope)
{
	std::string code = R"(
	
	int main()
	{
		struct A {int i;} a;
		{
			struct A { int j; };
			struct A a;
			a.j = 5;
		}
		a.i = 100;

		return a.i;
	}
	)";

	ExpectNoError(code);
}

TEST_F(StructValidationTest, err_incorrect_ret_type)
{
	std::string code = R"(
	struct B {int a; int b;};
	struct A {struct B ab; int i;};

	int main()
	{
		struct A a;

		a.ab.a = 1;
		a.ab.b = 10;
		a.i = 100;

		return a.ab;
	}
	)";

	ExpectError(code, ERR_INVALID_RETURN);
}

TEST_F(StructValidationTest, err_incorrect_type_assign)
{
	std::string code = R"(
	struct B {int a; int b;};
	struct A {struct B ab; int i;};

	int main()
	{
		struct A a;

		a.ab = 10;
		return 0;
	}
	)";

	ExpectError(code, ERR_INCOMPATIBLE_TYPE);
}

TEST_F(StructValidationTest, err_unknown_member)
{
	std::string code = R"(
	struct A {int a; int b;};

	int main()
	{
		struct A a;

		a.c = 10;
		return 0;
	}
	)";

	ExpectError(code, ERR_UNKNOWN_MEMBER_REF);
}

TEST_F(StructValidationTest, err_unknown_member2)
{
	std::string code = R"(
	struct B {int a; int b;};
	struct A {struct B ab; int i;};

	int main()
	{
		struct A a;

		a.ab.z = 10;
		return 0;
	}
	)";

	ExpectError(code, ERR_UNKNOWN_MEMBER_REF);
}

TEST_F(StructValidationTest, err_undefined_sizeof)
{
	std::string code = R"(
	int main()
	{		
		return sizeof(struct A);
	}
	)";

	ExpectError(code, ERR_UNKNOWN_TYPE);
}

TEST_F(StructValidationTest, defined_sizeof)
{
	std::string code = R"(
	int main()
	{		
		return sizeof(struct A{int i;});
	}
	)";

	ExpectNoError(code);
}

TEST_F(StructValidationTest, declare_define_new_scope)
{
	std::string code = R"(
	struct A;
	void foo()
	{		
		struct A {int i;} a;

		a.i = 1;
	}
	)";

	ExpectNoError(code);
}

TEST_F(StructValidationTest, err_declare_define_new_scope)
{
	std::string code = R"(
	struct A;
	void foo()
	{		
		struct A {int i;} a;

		a.i = 1;
	}

	void foo2()
	{		
		struct A a;

		a.i = 1;
	}

	)";

	ExpectError(code, ERR_TYPE_INCOMPLETE);
}

TEST_F(StructValidationTest, err_ptr_member_op_not_ptr)
{
	std::string code = R"(
	void foo()
	{		
		struct A {int i;} a;

		a->i = 1;
	})";

	ExpectError(code, ERR_INCOMPATIBLE_TYPE);
}

TEST_F(StructValidationTest, err_member_op_ptr)
{
	std::string code = R"(
	void foo()
	{		
		struct A {int i;}* a;

		a.i = 1;
	})";

	ExpectError(code, ERR_INCOMPATIBLE_TYPE);
}

TEST_F(StructValidationTest, err_member_op_not_user_type)
{
	std::string code = R"(
	void foo()
	{		
		int a;

		a.i = 1;
	})";

	ExpectError(code, ERR_INCOMPATIBLE_TYPE);
}

TEST_F(StructValidationTest, err_member_op_member_not_id)
{
	std::string code = R"(
	void foo()
	{		
		int a;
		int x;

		a.x = 1;
	})";

	ExpectError(code, ERR_INCOMPATIBLE_TYPE);
}