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
		EXPECT_EQ(return_type, decl->type_ref->spec->data.func_sig_spec->ret_type);
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
	ExpectError(ERR_SYNTAX);
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

TEST_F(ParstDeclTest, var_fn_ptr_typedef)
{
	ParseDecl("typedef int (*fn)(int);");
}

TEST_F(ParstDeclTest, array_type)
{
	ParseDecl("int p[5];");
	ASSERT_EQ(1, count);
	ExpectVarPtrDecl(0, "p", int32_type_spec);
}

TEST_F(ParstDeclTest, array_struct_bitsize)
{
	ParseDecl(R"(
	struct
	{
		int flag : 1;
	}s;)");
	
	AssertValid();
	ASSERT_EQ(1, count);
	ast_declaration_t* decl = DeclNo(0);
}
