#include "validation_fixture.h"

class StructAlignTest : public CompilerTest
{
public:
	StructAlignTest()
	{
		
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

	ast_struct_member_t* Member(const std::string& name)
	{
		return ast_find_struct_member(result->data.user_type_spec, name.c_str());
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

	size_t sz = result->size;
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

	size_t sz = result->size;
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

	size_t sz = result->size;
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

	size_t sz = result->size;
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

	size_t sz = result->size;
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

	size_t sz = result->size;
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

	size_t sz = result->size;
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

	size_t sz = result->size;
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

	size_t sz = result->size;
	EXPECT_EQ(40, sz);
	EXPECT_EQ(4, abi_get_type_alignment(result));
	EXPECT_EQ(0, MemberOffset("i1"));
}

TEST_F(StructAlignTest, struct_array)
{
	ParseStructDecl(R"(
	struct
	{
		struct {char c; short s;} m[10];
		
	};)");

	size_t sz = result->size;
	EXPECT_EQ(40, sz);
	EXPECT_EQ(4, abi_get_type_alignment(result));
	EXPECT_EQ(0, MemberOffset("m"));
}

//bit fields
TEST_F(StructAlignTest, bit_field_int)
{
	ParseStructDecl(R"(
	struct
	{
		int i1 : 1;
		int i2 : 1;
		int i3 : 16;
		
	};)");

	size_t sz = result->size;
	EXPECT_EQ(4, sz);
	EXPECT_EQ(4, abi_get_type_alignment(result));
	EXPECT_EQ(0, MemberOffset("i1"));
	EXPECT_EQ(0, MemberOffset("i2"));
	EXPECT_EQ(0, MemberOffset("i3"));

	EXPECT_EQ(0, Member("i1")->sema.bit_field.offset);
	EXPECT_EQ(1, Member("i1")->sema.bit_field.size);

	EXPECT_EQ(1, Member("i2")->sema.bit_field.offset);
	EXPECT_EQ(1, Member("i2")->sema.bit_field.size);

	EXPECT_EQ(2, Member("i3")->sema.bit_field.offset);
	EXPECT_EQ(16, Member("i3")->sema.bit_field.size);
}

TEST_F(StructAlignTest, bit_field_int_zero_len)
{
	ParseStructDecl(R"(
	struct
	{
		int i1 : 1;
		int i2 : 1;
		int : 0;
		//i3 should be placed in the next int
		int i3 : 16;
		
	};)");

	size_t sz = result->size;
	EXPECT_EQ(8, sz);
	EXPECT_EQ(4, abi_get_type_alignment(result));
	EXPECT_EQ(0, MemberOffset("i1"));
	EXPECT_EQ(0, MemberOffset("i2"));
	EXPECT_EQ(4, MemberOffset("i3"));

	EXPECT_EQ(0, Member("i1")->sema.bit_field.offset);
	EXPECT_EQ(1, Member("i1")->sema.bit_field.size);

	EXPECT_EQ(1, Member("i2")->sema.bit_field.offset);
	EXPECT_EQ(1, Member("i2")->sema.bit_field.size);

	EXPECT_EQ(0, Member("i3")->sema.bit_field.offset);
	EXPECT_EQ(16, Member("i3")->sema.bit_field.size);
}

TEST_F(StructAlignTest, bit_field_int_zero_len_initial)
{
	ParseStructDecl(R"(
	struct
	{
		int i1;
		int : 0;
		int i2 : 1;
		
	};)");

	size_t sz = result->size;
	EXPECT_EQ(8, sz);
	EXPECT_EQ(4, abi_get_type_alignment(result));
	EXPECT_EQ(0, MemberOffset("i1"));
	EXPECT_EQ(4, MemberOffset("i2"));

	EXPECT_EQ(0, Member("i2")->sema.bit_field.offset);
	EXPECT_EQ(1, Member("i2")->sema.bit_field.size);
}

TEST_F(StructAlignTest, bit_field_short)
{
	ParseStructDecl(R"(
	struct
	{
		short i1 : 1;
		short i2 : 8;
		short i3 : 2;
		
	};)");

	size_t sz = result->size;
	EXPECT_EQ(2, sz);
	EXPECT_EQ(2, abi_get_type_alignment(result));
	EXPECT_EQ(0, MemberOffset("i1"));
	EXPECT_EQ(0, MemberOffset("i2"));
	EXPECT_EQ(0, MemberOffset("i3"));

	EXPECT_EQ(0, Member("i1")->sema.bit_field.offset);
	EXPECT_EQ(1, Member("i1")->sema.bit_field.size);

	EXPECT_EQ(1, Member("i2")->sema.bit_field.offset);
	EXPECT_EQ(8, Member("i2")->sema.bit_field.size);

	EXPECT_EQ(9, Member("i3")->sema.bit_field.offset);
	EXPECT_EQ(2, Member("i3")->sema.bit_field.size);
}

TEST_F(StructAlignTest, bit_field_char)
{
	ParseStructDecl(R"(
	struct
	{
		char c1 : 1;
		char c2 : 6;
		char c3 : 1;
		
	};)");

	size_t sz = result->size;
	EXPECT_EQ(1, sz);
	EXPECT_EQ(1, abi_get_type_alignment(result));
	EXPECT_EQ(0, MemberOffset("c1"));
	EXPECT_EQ(0, MemberOffset("c2"));
	EXPECT_EQ(0, MemberOffset("c3"));

	EXPECT_EQ(0, Member("c1")->sema.bit_field.offset);
	EXPECT_EQ(1, Member("c1")->sema.bit_field.size);

	EXPECT_EQ(1, Member("c2")->sema.bit_field.offset);
	EXPECT_EQ(6, Member("c2")->sema.bit_field.size);

	EXPECT_EQ(7, Member("c3")->sema.bit_field.offset);
	EXPECT_EQ(1, Member("c3")->sema.bit_field.size);
}

TEST_F(StructAlignTest, bit_field_mix)
{
	ParseStructDecl(R"(
	struct A
	{
		int i;
		short x : 3;
		char y : 1;
		struct A* p;
	} ;)");

	size_t sz = result->size;
	EXPECT_EQ(12, sz);
	EXPECT_EQ(4, abi_get_type_alignment(result));
	EXPECT_EQ(0, MemberOffset("i"));
	EXPECT_EQ(4, MemberOffset("x"));
	EXPECT_EQ(4, MemberOffset("y"));
	EXPECT_EQ(8, MemberOffset("p"));

	EXPECT_EQ(0, Member("x")->sema.bit_field.offset);
	EXPECT_EQ(3, Member("x")->sema.bit_field.size);

	EXPECT_EQ(3, Member("y")->sema.bit_field.offset);
	EXPECT_EQ(1, Member("y")->sema.bit_field.size);
}

TEST_F(StructAlignTest, bit_field_example1)
{
	ParseStructDecl(R"(
	struct A
	{
		short s : 9;
		int i : 9;
		char c;
} ;)");

	size_t sz = result->size;
	EXPECT_EQ(4, sz);
	EXPECT_EQ(4, abi_get_type_alignment(result));

	EXPECT_EQ(0, MemberOffset("s"));
	EXPECT_EQ(0, MemberOffset("i"));
	EXPECT_EQ(3, MemberOffset("c"));

	EXPECT_EQ(0, Member("s")->sema.bit_field.offset);
	EXPECT_EQ(9, Member("s")->sema.bit_field.size);

	EXPECT_EQ(9, Member("i")->sema.bit_field.offset);
	EXPECT_EQ(9, Member("i")->sema.bit_field.size);
}