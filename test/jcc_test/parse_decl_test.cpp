#include "validation_fixture.h"

extern "C"
{
#include <libcomp/include/parse_internal.h>
#include <libcomp/include/std_types.h>
}

class ParstDeclTest : public CompilerTest
{
public:
	ast_declaration_t* ParseDecl(const std::string& code)
	{
		SetSource(code);
		Lex();

		parse_init(mTokens);

		decl = try_parse_declaration();
		return decl;
	}

	void AssertValid()
	{
		ASSERT_NE(nullptr, decl);
	}

	/*void AssertSpecKind(type_kind k)
	{
		AssertValid();
		ASSERT_EQ(k, type_ref->spec->kind);
	}

	void ExpectPtrTo(type_kind k)
	{
		AssertSpecKind(type_ptr);
		EXPECT_EQ(k, type_ref->spec->data.ptr_type->kind);
	}

	void ExpectType(type_kind k, uint32_t flags = 0)
	{
		AssertValid();
		EXPECT_EQ(k, type_ref->spec->kind);
		EXPECT_EQ(flags, type_ref->flags);
	}

	void ExpectUserType(user_type_kind ut, uint32_t flags = 0)
	{
		AssertValid();
		ExpectType(type_user, flags);
		EXPECT_EQ(ut, type_ref->spec->data.user_type_spec->kind);
	}

	void ExpectUserTypeName(const char* name)
	{
		AssertValid();
		EXPECT_THAT(type_ref->spec->data.user_type_spec->name, StrEq(name));
	}*/

	void ExpectVarDeclName(const std::string& name)
	{
		ASSERT_EQ(decl_var, decl->kind);
		EXPECT_EQ(name, decl->name);
	}

	void ExpectVarDeclType(ast_type_spec_t* spec)
	{
		ASSERT_EQ(decl_var, decl->kind);
		EXPECT_EQ(spec, decl->type_ref->spec);
	}

	ast_declaration_t* decl = nullptr;
};

TEST_F(ParstDeclTest, global_int)
{
	ParseDecl("int j;");
	AssertValid();
	ExpectVarDeclName("j");
	ExpectVarDeclType(int32_type_spec);
}
