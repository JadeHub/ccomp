#include "validation_fixture.h"

extern "C"
{
#include <libcomp/include/parse_internal.h>
#include <libcomp/include/std_types.h>
}

class ParstDeclTest : public CompilerTest
{
public:
	void ParseDecl(const std::string& code, decl_parse_context context = dpc_normal)
	{
		SetSource(code);
		Lex();

		parse_init(mTokens);

		decls = try_parse_decl_list(context);
		ast_declaration_t* decl = decls.first;

		while (decl)
		{
			count++;
			decl = decl->next;
		}
	}

	void AssertValid()
	{
		ASSERT_NE(nullptr, decls.first);
	}

	void ExpectArraySize(ast_type_spec_t* spec, size_t element_count)
	{
		ASSERT_EQ(type_array, spec->kind);
		ASSERT_EQ(expr_int_literal, spec->data.array_spec->size_expr->kind);
		EXPECT_EQ(element_count, (size_t)spec->data.array_spec->size_expr->data.int_literal.val.v.uint64);
	}

	void ExpectArrayDecl(size_t idx, const std::string& name, size_t element_count, uint32_t type_flags = 0)
	{
		ast_declaration_t* decl = DeclNo(idx);
		ASSERT_NE(nullptr, decl);
		EXPECT_EQ(decl_var, decl->kind);
		EXPECT_EQ(name, decl->name);
		ASSERT_EQ(type_array, decl->type_ref->spec->kind);
		ExpectArraySize(decl->type_ref->spec, element_count);
		EXPECT_EQ(type_flags, decl->type_ref->flags);
	}

	void ExpectVarDecl(size_t idx, const std::string& name, ast_type_spec_t* spec, uint32_t type_flags = 0)
	{
		ast_declaration_t* decl = DeclNo(idx);
		ASSERT_NE(nullptr, decl);
		EXPECT_EQ(decl_var, decl->kind);
		EXPECT_EQ(name, decl->name);
		EXPECT_EQ(spec, decl->type_ref->spec);
		EXPECT_EQ(type_flags, decl->type_ref->flags);
	}

	void ExpectVarPtrDecl(size_t idx, const std::string& name, ast_type_spec_t* ptr_type)
	{
		ast_declaration_t* decl = DeclNo(idx);
		ASSERT_NE(nullptr, decl);
		EXPECT_EQ(decl_var, decl->kind);
		EXPECT_EQ(name, decl->name);
		ASSERT_EQ(type_ptr, decl->type_ref->spec->kind);
		EXPECT_EQ(ptr_type, decl->type_ref->spec->data.ptr_type);
	}

	void ExpectFnDecl(size_t idx, const std::string& name, ast_type_spec_t* return_type)
	{
		ast_declaration_t* decl = DeclNo(idx);
		ASSERT_NE(nullptr, decl);
		EXPECT_EQ(decl_func, decl->kind);
		EXPECT_EQ(name, decl->name);
		ASSERT_EQ(type_func_sig, decl->type_ref->spec->kind);
		EXPECT_TRUE(sema_is_same_type(return_type, decl->type_ref->spec->data.func_sig_spec->ret_type));
	}

	ast_declaration_t* DeclNo(size_t i)
	{
		ast_declaration_t* result = decls.first;

		while (i)
		{
			result = result->next;
			i--;
		}
		return result;
	}

	ast_decl_list_t decls;
	size_t count = 0;
};

TEST_F(ParstDeclTest, global_int)
{
	ParseDecl("int j;");
	AssertValid();
	ASSERT_EQ(1, count);
	ExpectVarDecl(0, "j", int32_type_spec);
}

TEST_F(ParstDeclTest, global_const_int)
{
	ParseDecl("int const j, i;");
	AssertValid();
	ASSERT_EQ(2, count);
	ExpectVarDecl(0, "j", int32_type_spec, TF_QUAL_CONST);
	ExpectVarDecl(1, "i", int32_type_spec, TF_QUAL_CONST);
}

TEST_F(ParstDeclTest, global_int_list)
{
	ParseDecl("int j,* i;");
	AssertValid();
	ASSERT_EQ(2, count);
	ExpectVarDecl(0, "j", int32_type_spec);
	ExpectVarPtrDecl(1, "i", int32_type_spec);
}

TEST_F(ParstDeclTest, err_const_after_spec)
{
	TestWithErrorHandling::ExpectError(ERR_SYNTAX);
	ParseDecl("int a, const b;");
}

TEST_F(ParstDeclTest, var_fn_var_decl)
{
	ParseDecl("int a, b(), c;");
	AssertValid();
	ASSERT_EQ(3, count);

	ExpectVarDecl(0, "a", int32_type_spec);
	ExpectFnDecl(1, "b", int32_type_spec);
	ExpectVarDecl(2, "c", int32_type_spec);
}

TEST_F(ParstDeclTest, var_fn_ptr_decl)
{
	ParseDecl("int (*fn)(char, int);");
	AssertValid();
	ASSERT_EQ(1, count);
}

TEST_F(ParstDeclTest, fn_ret_ptr)
{
	ParseDecl("int* fn()");
	AssertValid();
	ASSERT_EQ(1, count);
	ExpectFnDecl(0, "fn", ast_make_ptr_type(int32_type_spec));
}

TEST_F(ParstDeclTest, array_of_ptrs)
{
	ParseDecl("int *p[5];");
	AssertValid();
	ASSERT_EQ(1, count);
	ExpectArrayDecl(0, "p", 5);
	ast_type_spec_t* spec = DeclNo(0)->type_ref->spec;
	//element type is pointer to int
	EXPECT_EQ(type_ptr, spec->data.array_spec->element_type->kind);
	EXPECT_EQ(int32_type_spec, spec->data.array_spec->element_type->data.ptr_type);
}

TEST_F(ParstDeclTest, array_type)
{
	ParseDecl("int p[5];");
	AssertValid();
	ASSERT_EQ(1, count);
	ExpectArrayDecl(0, "p", 5);
	ast_type_spec_t* spec = DeclNo(0)->type_ref->spec;
	EXPECT_EQ(spec->data.array_spec->element_type, int32_type_spec);
}

TEST_F(ParstDeclTest, multi_d_array_type)
{
	ParseDecl("int p[5][2];");
	AssertValid();
	ASSERT_EQ(1, count);

	//p is an array containing 5 elements
	ExpectArrayDecl(0, "p", 5);
	ast_type_spec_t* spec = DeclNo(0)->type_ref->spec;
	sema_resolve_type(spec, DeclNo(0)->tokens.start);
	EXPECT_EQ(40, spec->size);
	//each of those elements are an array of int32 2 elements long
	ast_type_spec_t* elem_spec = spec->data.array_spec->element_type;
	sema_resolve_type(elem_spec, DeclNo(0)->tokens.start);
	ASSERT_EQ(elem_spec->kind, type_array);
	ExpectArraySize(elem_spec, 2);
	EXPECT_EQ(8, elem_spec->size);
}

TEST_F(ParstDeclTest, multi_d_array_type2)
{
	ParseDecl("int p[2][3][4];");
	AssertValid();
	ASSERT_EQ(1, count);

	//p is an array containing 2 elements
	ExpectArrayDecl(0, "p", 2);
	ast_type_spec_t* spec = DeclNo(0)->type_ref->spec;
	sema_resolve_type(spec, DeclNo(0)->tokens.start);
	EXPECT_EQ(96, spec->size);
	//each of those elements are an array containing 3 elements
	ast_type_spec_t* elem_spec = spec->data.array_spec->element_type;
	sema_resolve_type(elem_spec, DeclNo(0)->tokens.start);
	ASSERT_EQ(elem_spec->kind, type_array);
	ExpectArraySize(elem_spec, 3);
	//EXPECT_EQ(8, elem_spec->size);
}

TEST_F(ParstDeclTest, array_ptr)
{
	ParseDecl("int (*p)[5];");
	AssertValid();
	ASSERT_EQ(1, count);

	ast_declaration_t* decl = DeclNo(0);
	ast_type_spec_t* spec = decl->type_ref->spec;
	//decl is a pointer
	ASSERT_TRUE(ast_type_is_ptr(spec));
	//..to an array
	ast_type_spec_t* array_type = spec->data.ptr_type;
	ASSERT_TRUE(ast_type_is_array(array_type));
	//..of ints
	ASSERT_TRUE(sema_is_same_type(array_type->data.array_spec->element_type, int32_type_spec));
	//... with lenth 5
	ast_expression_t* sz_expr = array_type->data.array_spec->size_expr;
	ASSERT_TRUE(sz_expr->kind == expr_int_literal);
	ASSERT_EQ(5, sz_expr->data.int_literal.val.v.int64);
}

TEST_F(ParstDeclTest, typedef_fn_ptr)
{
	ParseDecl("typedef int* (*fn_name)();");
	AssertValid();
	//1 declaration
	ASSERT_EQ(1, count);
	ast_declaration_t* decl = DeclNo(0);
	//it is a typedef
	ASSERT_TRUE(decl->type_ref->flags & TF_SC_TYPEDEF);
	ASSERT_TRUE(decl->kind == decl_type);
	//.. named "fn_name"
	ASSERT_STREQ("fn_name", decl->name);
	ast_type_spec_t* spec = decl->type_ref->spec;
	//...whic is a pointer to a function
	ASSERT_TRUE(ast_type_is_fn_ptr(spec));
	//...which returns int*
	ast_type_spec_t* fn_type = spec->data.ptr_type;
	ASSERT_TRUE(sema_is_same_type(fn_type->data.func_sig_spec->ret_type, ast_make_ptr_type(int32_type_spec)));
	//...and takes no params
	ASSERT_TRUE(fn_type->data.func_sig_spec->params->param_count == 0);
}