#include "validation_fixture.h"

extern "C"
{
#include <libcomp/include/parse_internal.h>
}

class ParstTypeTest : public CompilerTest
{
public:
	ast_type_ref_t* ParseType(const std::string& code)
	{
		SetSource(code);
		Lex();

		parse_init(mTokens);
		
		type_ref = try_parse_type_ref();
		return type_ref;
	}

	void AssertValid()
	{
		ASSERT_NE(nullptr, type_ref);
		ASSERT_NE(nullptr, type_ref->spec);
	}

	void AssertSpecKind(type_kind k)
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
	}
	
	ast_type_ref_t* type_ref = nullptr;
};

TEST_F(ParstTypeTest, Int8)
{
	ParseType("char");
	ExpectType(type_int8);

	ParseType("signed char");
	ExpectType(type_int8);

	ParseType("char *");
	ExpectPtrTo(type_int8);
}

TEST_F(ParstTypeTest, UInt8)
{
	ParseType("unsigned char");
	ExpectType(type_uint8);
}

TEST_F(ParstTypeTest, Int16)
{
	ParseType("short");
	ExpectType(type_int16);

	ParseType("signed short");
	ExpectType(type_int16);

	ParseType("signed short int");
	ExpectType(type_int16);
}

TEST_F(ParstTypeTest, UInt16)
{
	ParseType("unsigned short");
	ExpectType(type_uint16);

	ParseType("unsigned short int");
	ExpectType(type_uint16);
}

TEST_F(ParstTypeTest, Int32)
{
	ParseType("int");
	ExpectType(type_int32);

	ParseType("signed");
	ExpectType(type_int32);

	ParseType("signed int");
	ExpectType(type_int32);

	ParseType("signed long");
	ExpectType(type_int32);

	ParseType("signed long int");
	ExpectType(type_int32);
}

TEST_F(ParstTypeTest, UInt32)
{
	ParseType("unsigned int");
	ExpectType(type_uint32);

	ParseType("unsigned");
	ExpectType(type_uint32);

	ParseType("unsigned long");
	ExpectType(type_uint32);

	ParseType("unsigned long int");
	ExpectType(type_uint32);
}

TEST_F(ParstTypeTest, Int64)
{
	ParseType("long long");
	ExpectType(type_int64);

	ParseType("signed long long");
	ExpectType(type_int64);

	ParseType("signed long long int");
	ExpectType(type_int64);
}

TEST_F(ParstTypeTest, UInt64)
{
	ParseType("unsigned long long");
	ExpectType(type_uint64);

	ParseType("unsigned long long int");
	ExpectType(type_uint64);

	ParseType("unsigned long const long static int");
	ExpectType(type_uint64, TF_QUAL_CONST | TF_SC_STATIC);
}

TEST_F(ParstTypeTest, user_struct)
{
	ParseType("struct Abc {int i;}");
	ExpectUserType(user_type_struct);
	ExpectUserTypeName("Abc");

	ParseType("struct {int i;} const");
	ExpectType(type_user, TF_QUAL_CONST);

	//ParseType("struct const {int i;}");
	//ExpectType(type_user, TF_Q_CONST);
}


