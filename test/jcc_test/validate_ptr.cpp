#include "validation_fixture.h"

class PtrValidationTest : public CompilerTest {};

TEST_F(PtrValidationTest, global_ptr)
{
	std::string code = R"(int* p;)";

	ExpectNoError(code);
}

TEST_F(PtrValidationTest, local_ptr)
{
	std::string code = R"(
						void fn()
						{
							int* i;
						}
						)";

	ExpectNoError(code);
}

TEST_F(PtrValidationTest, local_ptr_ptr)
{
	std::string code = R"(
						void fn()
						{
							int** i;
						}
						)";

	ExpectNoError(code);
}

TEST_F(PtrValidationTest, return_ptr)
{
	std::string code = R"(int g = 5; 
						int* fn()
						{
							return &g;
						})";

	ExpectNoError(code);
}

TEST_F(PtrValidationTest, param_ptr)
{
	std::string code = R"(
						void fn(int* i, char* c)
						{
						})";

	ExpectNoError(code);
}

TEST_F(PtrValidationTest, struct_mem_ptr)
{
	std::string code = R"(
						struct A
						{
							int* member;
						};
						)";

	ExpectNoError(code);
}

TEST_F(PtrValidationTest, global_array)
{
	std::string code = R"(int p[5];)";

	ExpectNoError(code);
}

TEST_F(PtrValidationTest, string_literal)
{
	std::string code = R"(char *p = "Hello";)";

	ExpectNoError(code);
}

TEST_F(PtrValidationTest, assign_null)
{
	std::string code = R"(
	void fn()
	{
		int* p = ((void*)0);
	}
)";

	ExpectNoError(code);
}

TEST_F(PtrValidationTest, assign_null_global)
{
	std::string code = R"(
	int* p = ((void*)0);
)";

	ExpectNoError(code);
}


TEST_F(PtrValidationTest, return_void_ptr)
{
	std::string code = R"(
	void* fn();
	int* test()
	{
		return fn();
	}
)";

	ExpectNoError(code);
}