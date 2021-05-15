#include "validation_fixture.h"

class FnPtrSmntValidationTest : public CompilerTest {};

TEST_F(FnPtrSmntValidationTest, assign_fn)
{
	std::string code = R"(
	void foo() {}

	void test()
	{
		void (*fn)() = foo;
		void (*fn1)();

		fn1 = fn;
	}
	
	)";

	ExpectNoError(code);
}

TEST_F(FnPtrSmntValidationTest, call)
{
	std::string code = R"(
	void foo() {}

	void test()
	{
		void (*fn)() = foo;
		fn();

	}
	
	)";

	ExpectNoError(code);
}

TEST_F(FnPtrSmntValidationTest, call_via_typedef)
{
	std::string code = R"(
	

	typedef struct a {int i;} a_t;
	typedef a_t* (*fn_ptr_t)();

	a_t* foo();

	void test()
	{
		fn_ptr_t fn = foo;
		fn();
	}
	
	)";

	ExpectNoError(code);
}

TEST_F(FnPtrSmntValidationTest, assign_fn_params)
{
	std::string code = R"(
	int f(int a, char* f)
	{
		return a + f[0];
	}

	int main()
	{
		int (*p)(int, char*) = f;
		return 0;
	})";
	ExpectNoError(code);
}