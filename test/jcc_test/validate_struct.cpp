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

TEST_F(StructValidationTest, anonymous)
{
	std::string code = R"(
	
	int main()
	{
		struct {int i; int j;} a;
		a.i = 5;
		
		struct {int x; int y;} b;
		a.i = 5;
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

TEST_F(StructValidationTest, inline_decl)
{
	std::string code = R"(
	struct A {int i;}; 
	struct A j;
	)";

	ExpectNoError(code);
}

TEST_F(StructValidationTest, typedef_struct)
{
	std::string code = R"(
	typedef struct {int i;} A;
	A a;

	void fn() 
	{
		a.i = 7;
	}

	)";

	ExpectNoError(code);
}

TEST_F(StructValidationTest, typedef_struct_return)
{
	std::string code = R"(
	typedef struct {int i;} A;
	A a;

	A fn() 
	{
		a.i = 7;
		return a;
	}

	)";

	ExpectNoError(code);
}

TEST_F(StructValidationTest, err_typedef_struct_dup)
{
	std::string code = R"(
	typedef struct {int i;} A;
	typedef struct {char c;} A;
	A a;

	void fn() 
	{
		a.i = 7;
	}

	)";

	ExpectError(code, ERR_DUP_TYPE_DEF);
}

TEST_F(StructValidationTest, err_duplicate_member_name)
{
	std::string code = R"(
	struct a
	{
		int member;
		int member;
	};

	)";

	ExpectError(code, ERR_DUP_SYMBOL);
}

TEST_F(StructValidationTest, err_member_incomplete_type)
{
	std::string code = R"(
	struct B;
	struct A
	{
		struct B b;
	};

	)";

	ExpectError(code, ERR_TYPE_INCOMPLETE);
}

TEST_F(StructValidationTest, err_member_non_const_bitsize)
{
	std::string code = R"(
	
	struct A
	{
		int i : p;
	};

	)";

	ExpectError(code, ERR_INITIALISER_NOT_CONST);
}

TEST_F(StructValidationTest, err_member_bitsize_too_large)
{
	std::string code = R"(
	
	struct A
	{
		int i : 33;
	};

	)";

	ExpectError(code, ERR_UNSUPPORTED);
}

TEST_F(StructValidationTest, err_member_bitsize_negative)
{
	std::string code = R"(
	
	struct A
	{
		int i : -1;
	};

	)";

	ExpectError(code, ERR_UNSUPPORTED);
}

TEST_F(StructValidationTest, err_member_bitsize_invalid_type)
{
	std::string code = R"(
	
	struct A
	{
		char* i : 1;
	};

	)";

	ExpectError(code, ERR_UNSUPPORTED);
}

TEST_F(StructValidationTest, err_member_bitsize_zero_named)
{
	std::string code = R"(
	
	struct A
	{
		int i : 0;
	};

	)";

	ExpectError(code, ERR_UNSUPPORTED);
}

TEST_F(StructValidationTest, member_bit_sizes)
{
	std::string code = R"(
	
	struct A
	{
		int i : 1;
		int j : 5;
		int : 32 - (5 + 1);
		int x : 30;
		int : 1;
		int y : 1;
	};

	)";

	ExpectNoError(code);
}

TEST_F(StructValidationTest, err_address_of_bit_field)
{
	std::string code = R"(
	
	void fn()
	{
		struct A { int i : 1;} a;
		int* p = &a.i;
	}

	)";

	ExpectError(code, ERR_INCOMPATIBLE_TYPE);
}

TEST_F(StructValidationTest, typedef_struct_self)
{
	std::string code = R"(
	
	typedef struct A{struct A* ptr;} a_t;

	void fn()
	{
		a_t a1, a2;
		a1.ptr = &a2;
	}

	)";

	ExpectNoError(code);
}

TEST_F(StructValidationTest, struct_init)
{
	std::string code = R"(
	
	void fn()
	{
		struct A {int i; int j;} v = {1, 2};
	}

	)";

	ExpectNoError(code);
}

TEST_F(StructValidationTest, global_struct_init)
{
	std::string code = R"(
	
	struct A {int i; int j;} v = {1, 2};

	)";

	ExpectNoError(code);
}

TEST_F(StructValidationTest, err_struct_init_to_few)
{
	std::string code = R"(
	
	void fn()
	{
		struct A {int i; int j;} v = {1};
	}

	)";

	ExpectError(code, ERR_SYNTAX);
}

TEST_F(StructValidationTest, err_struct_init_to_many)
{
	std::string code = R"(
	
	void fn()
	{
		struct A {int i; int j;} v = {1, 2, 3};
	}

	)";

	ExpectError(code, ERR_SYNTAX);
}

TEST_F(StructValidationTest, nested_struct_init)
{
	std::string code = R"(
	
	void fn()
	{
		struct A
		{
			int i;
			struct {char c1; char c2;} n;
			int j;
		} v = {1, {'a', 'b'}, 2};
	}

	)";

	ExpectNoError(code);
}

TEST_F(StructValidationTest, err_member_unknown_array_len)
{
	std::string code = R"(
	struct A
	{
		int i[];
	};

	)";

	ExpectError(code, ERR_UNSUPPORTED);
}