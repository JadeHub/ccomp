#include "validation_fixture.h"

class StructAlignTest : public CompilerTest
{
public:
	StructAlignTest()
	{
		sema_init();
	}

	void ParseStructDecl(const std::string& code)
	{
		SetSource(code);
		Lex();

		parse_init(mTokens);

		ast_decl_list_t decls = try_parse_decl_list(dpc_normal);
		ASSERT_NE(nullptr, decls.first);
		ASSERT_TRUE(sema_resolve_type_ref(decls.first->type_ref));
		result = decls.first->type_ref->spec;
	}

	size_t MemberOffset(const std::string& name)
	{
		auto member = ast_find_struct_member(result->data.user_type_spec, name.c_str());
		return member->sema.offset;
	}

	ast_type_spec_t* result = nullptr;
};

TEST_F(StructAlignTest, struct_single_char)
{
	ParseStructDecl(R"(
	struct
	{
		char c1;
	};)");

	size_t sz = abi_calc_user_type_layout(result);
	EXPECT_EQ(1, sz);
	EXPECT_EQ(1, abi_get_type_alignment(result));
	EXPECT_EQ(0, MemberOffset("c1"));
}

TEST_F(StructAlignTest, struct_char_char_char)
{
	ParseStructDecl(R"(
	struct
	{
		char c1;
		char c2;
		char c3;
	};)");

	size_t sz = abi_calc_user_type_layout(result);
	EXPECT_EQ(3, sz);
	EXPECT_EQ(1, abi_get_type_alignment(result));
	EXPECT_EQ(0, MemberOffset("c1"));
	EXPECT_EQ(1, MemberOffset("c2"));
	EXPECT_EQ(2, MemberOffset("c3"));
}

TEST_F(StructAlignTest, struct_char_short)
{
	ParseStructDecl(R"(
	struct
	{
		char c1;
		short s1;
	};)");

	size_t sz = abi_calc_user_type_layout(result);
	EXPECT_EQ(4, sz);
	EXPECT_EQ(2, abi_get_type_alignment(result));
	EXPECT_EQ(0, MemberOffset("c1"));
	EXPECT_EQ(2, MemberOffset("s1"));
}

TEST_F(StructAlignTest, struct_single_short)
{
	ParseStructDecl(R"(
	struct
	{
		short s1;
	};)");

	size_t sz = abi_calc_user_type_layout(result);
	EXPECT_EQ(2, sz);
	EXPECT_EQ(2, abi_get_type_alignment(result));
	EXPECT_EQ(0, MemberOffset("s1"));
}

TEST_F(StructAlignTest, struct_single_int)
{
	ParseStructDecl(R"(
	struct
	{
		int i1;
	};)");

	size_t sz = abi_calc_user_type_layout(result);
	EXPECT_EQ(4, sz);
	EXPECT_EQ(4, abi_get_type_alignment(result));
	EXPECT_EQ(0, MemberOffset("i1"));
}

TEST_F(StructAlignTest, struct_char_short_char)
{
	//alignment is 2 due to the short so we expect a single byte of padding at the end

	ParseStructDecl(R"(
	struct
	{
		char c1;
		short s1;
		char c2;
	};)");

	size_t sz = abi_calc_user_type_layout(result);
	EXPECT_EQ(6, sz);
	EXPECT_EQ(2, abi_get_type_alignment(result));
	EXPECT_EQ(0, MemberOffset("c1"));
	EXPECT_EQ(2, MemberOffset("s1"));
	EXPECT_EQ(4, MemberOffset("c2"));
}

TEST_F(StructAlignTest, struct_char_int_char)
{
	ParseStructDecl(R"(
	struct
	{
		char c1;
		int i1;
		char c2;
	};)");

	size_t sz = abi_calc_user_type_layout(result);
	EXPECT_EQ(12, sz);
	EXPECT_EQ(4, abi_get_type_alignment(result));
	EXPECT_EQ(0, MemberOffset("c1"));
	EXPECT_EQ(4, MemberOffset("i1"));
	EXPECT_EQ(8, MemberOffset("c2"));
}

TEST_F(StructAlignTest, int_ptr)
{
	ParseStructDecl(R"(
	struct
	{
		int i1;
		struct A* p1;
		
	};)");

	size_t sz = abi_calc_user_type_layout(result);
	EXPECT_EQ(8, sz);
	EXPECT_EQ(4, abi_get_type_alignment(result));
	EXPECT_EQ(0, MemberOffset("i1"));
	EXPECT_EQ(4, MemberOffset("p1"));
}

TEST_F(StructAlignTest, int_array)
{
	ParseStructDecl(R"(
	struct
	{
		int i1[10];
		
	};)");

	size_t sz = abi_calc_user_type_layout(result);
	EXPECT_EQ(40, sz);
}

struct
{
	int vals[10];
}t_1;
