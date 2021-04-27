#include "parse_internal.h"
#include "diag.h"
#include "std_types.h"

#include <libj/include/hash_table.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>

typedef struct alias_name_set
{
	hash_table_t* names;
	struct alias_name_set* next;
}alias_name_set_t;

static alias_name_set_t* _alias_name_stack;

static alias_name_set_t* _alloc_alias_name_set()
{
	alias_name_set_t* result = (alias_name_set_t*)malloc(sizeof(alias_name_set_t));
	memset(result, 0, sizeof(alias_name_set_t));
	result->names = sht_create(64);
	return result;
}

void parse_on_enter_block()
{
	alias_name_set_t* set = _alloc_alias_name_set();
	set->next = _alias_name_stack;
	_alias_name_stack = set;
}

void parse_on_leave_block()
{
	alias_name_set_t* set = _alias_name_stack;
	_alias_name_stack = _alias_name_stack->next;
	ht_destroy(set->names);
	free(set);
}

void parse_register_alias_name(const char* name)
{
	sht_insert(_alias_name_stack->names, name, NULL);
}

static bool _is_alias_name(const char* name)
{
	alias_name_set_t* set = _alias_name_stack;

	while(set)
	{
		if (sht_contains(set->names, name))
			return true;
		set = set->next;
	}
	return false;
}

//type-specifiers
#define DECL_SPEC_VOID			(1 << 1)
#define DECL_SPEC_CHAR			(1 << 2)
#define DECL_SPEC_SHORT			(1 << 3)
#define DECL_SPEC_INT			(1 << 4)
#define DECL_SPEC_LONG			(1 << 5)
#define DECL_SPEC_LONG_LONG		(1 << 6)
#define DECL_SPEC_SIGNED		(1 << 7)
#define DECL_SPEC_UNSIGNED		(1 << 8)
#define DECL_SPEC_FLOAT			(1 << 9)
#define DECL_SPEC_DOUBLE		(1 << 10)
#define DECL_SPEC_STRUCT		(1 << 11)
#define DECL_SPEC_UNION			(1 << 12)
#define DECL_SPEC_ENUM			(1 << 13)

#define DECL_SPEC_TYPE_FLAGS	0x3FFE

static bool _is_type_qualifier(token_t* tok)
{
	switch (tok->kind)
	{
	case tok_const:
	case tok_volatile:
		return true;
	}
	return false;
}

static bool _is_type_specifier(token_t* tok)
{
	switch (tok->kind)
	{
	case tok_void:
	case tok_char:
	case tok_short:
	case tok_int:
	case tok_long:
	case tok_signed:
	case tok_unsigned:
	case tok_float:
	case tok_double:
	case tok_struct:
	case tok_union:
	case tok_enum:
	case tok_identifier:
		return true;
	};
	return false;
}

static bool _is_numeric_type_specifier(token_t* tok)
{
	switch (tok->kind)
	{
	case tok_char:
	case tok_short:
	case tok_int:
	case tok_long:
	case tok_signed:
	case tok_unsigned:
	case tok_float:
	case tok_double:
		return true;
	};
	return false;
}

static bool _is_user_type_specifier(token_t* tok)
{
	switch (tok->kind)
	{
	case tok_struct:
	case tok_union:
	case tok_enum:
		return true;
	};
	return false;
}

static bool _is_storage_class_specifier(token_t* tok)
{
	switch (tok->kind)
	{
	case tok_typedef:
	case tok_extern:
	case tok_static:
	case tok_auto:
	case tok_register:
	case tok_inline:
		return true;
	};
	return false;
}

static uint32_t _get_decl_spec_flag(token_t* tok)
{
	switch (tok->kind)
	{
	case tok_void:
		return DECL_SPEC_VOID;
	case tok_char:
		return DECL_SPEC_CHAR;
	case tok_short:
		return DECL_SPEC_SHORT;
	case tok_int:
		return DECL_SPEC_INT;
	case tok_long:
		return DECL_SPEC_LONG;
	case tok_signed:
		return DECL_SPEC_SIGNED;
	case tok_unsigned:
		return DECL_SPEC_UNSIGNED;
	case tok_float:
		return DECL_SPEC_FLOAT;
	case tok_double:
		return DECL_SPEC_DOUBLE;
	case tok_struct:
		return DECL_SPEC_STRUCT;
	case tok_union:
		return DECL_SPEC_UNION;
	case tok_enum:
		return DECL_SPEC_ENUM;
	}
	return 0;
}

static bool _is_type_spec(uint32_t flags, uint32_t type_flags)
{
	return ((flags & DECL_SPEC_TYPE_FLAGS) == type_flags);
}

/*
<enum_specifier> :: = "enum" [<id>] ["{" { <id> [= <const_expr>] } "}"]
*/
ast_user_type_spec_t* parse_enum_spec()
{
	ast_user_type_spec_t* result = (ast_user_type_spec_t*)malloc(sizeof(ast_user_type_spec_t));
	memset(result, 0, sizeof(ast_user_type_spec_t));
	result->tokens.start = current();
	result->kind = user_type_enum;

	if (current_is(tok_identifier))
	{
		tok_spelling_cpy(current(), result->name, MAX_LITERAL_NAME);
		next_tok();
	}

	if (current_is(tok_l_brace))
	{
		next_tok();

		ast_enum_member_t* last_member = NULL;

		while (true)
		{
			if (!expect_cur(tok_identifier))
				goto _enum_parse_err;

			ast_enum_member_t* member = (ast_enum_member_t*)malloc(sizeof(ast_enum_member_t));
			memset(member, 0, sizeof(ast_enum_member_t));
			member->tokens.start = current();
			tok_spelling_cpy(current(), member->name, MAX_LITERAL_NAME);
			next_tok();

			if (current_is(tok_equal))
			{
				next_tok();
				member->value = parse_constant_expression();
				if (!member->value)
				{
					parse_err(ERR_SYNTAX, "error parsing enum initialisation expression");
					goto _enum_parse_err;
				}
			}

			if (!last_member)
			{
				result->data.enum_members = member;
				last_member = member;
			}
			else
			{
				last_member->next = member;
			}
			last_member = member;

			member->tokens.end = current();

			if (!current_is(tok_comma))
				break;
			next_tok();
		}
		if (!expect_cur(tok_r_brace))
			goto _enum_parse_err;
		next_tok();
	}
	result->tokens.end = current();
	return result;
_enum_parse_err:
	free(result);
	return NULL;
}

/*
<user_type_specifier> :: = ("struct" | "union") [<id>] ["{" < struct_decl_list > "}"]
*/
ast_user_type_spec_t* parse_struct_spec(user_type_kind kind)
{
	ast_user_type_spec_t* result = (ast_user_type_spec_t*)malloc(sizeof(ast_user_type_spec_t));
	memset(result, 0, sizeof(ast_user_type_spec_t));
	result->tokens.start = current();
	result->kind = kind;

	if (current_is(tok_identifier))
	{
		tok_spelling_cpy(current(), result->name, MAX_LITERAL_NAME);
		next_tok();
	}
	else if(!current_is(tok_l_brace))
	{
		//if there is no name there should be a definition
		parse_err(ERR_SYNTAX, "expected definition or tag name after %s",
			kind == user_type_struct ? "struct" : "union");
		free(result);
		return NULL;
	}

	if (current_is(tok_l_brace))
	{
		next_tok();
		//uint32_t offset = 0;
		ast_struct_member_t* last_member = NULL;
		while (!current_is(tok_r_brace))
		{
			/*
			Parse a list of declarations and turn each into an ast_struct_member_t
			
			eg...
			struct{
				...
				int a, b, *c;
				...
			};
			*/

			ast_decl_list_t decls = try_parse_decl_list(dpc_struct);
			if (!decls.first && !parse_seen_err())
				parse_err(ERR_SYNTAX, "expected declaration, found %s", diag_tok_desc(current()));

			expect_cur(tok_semi_colon);
			next_tok();

			ast_declaration_t* decl = decls.first;

			while (decl)
			{
				ast_struct_member_t* member = (ast_struct_member_t*)malloc(sizeof(ast_struct_member_t));
				memset(member, 0, sizeof(ast_struct_member_t));
				member->tokens = decl->tokens;
				member->decl = decl;

				if (last_member == NULL)
				{
					result->data.struct_members = member;
				}
				else
				{
					last_member->next = member;
				}
				last_member = member;
				decl = decl->next;
			}
		}
		next_tok();
	}
	result->tokens.end = current();
	return result;
}

static inline ast_type_spec_t* _alloc_type_spec()
{
	ast_type_spec_t* result = (ast_type_spec_t*)malloc(sizeof(ast_type_spec_t));
	memset(result, 0, sizeof(ast_type_spec_t));
	return result;
}

ast_user_type_spec_t* parse_user_type()
{
	if (current()->kind == tok_struct)
	{
		next_tok();
		return parse_struct_spec(user_type_struct);
	}
	else if (current()->kind == tok_union)
	{
		next_tok();
		return parse_struct_spec(user_type_union);
	}
	else if (current()->kind == tok_enum)
	{
		next_tok();
		return parse_enum_spec();
	}
	return NULL;
}

ast_type_spec_t* _interpret_numeric_type_flags(uint32_t spec_flags)
{	
	//default to signed
	if (!(spec_flags & DECL_SPEC_UNSIGNED))
		spec_flags |= DECL_SPEC_SIGNED;

	/*
	8bit integer
	signed char
	unsigned char
	*/
	if (_is_type_spec(spec_flags, DECL_SPEC_SIGNED | DECL_SPEC_CHAR) ||
		_is_type_spec(spec_flags, DECL_SPEC_UNSIGNED | DECL_SPEC_CHAR))
	{
		return (spec_flags & DECL_SPEC_UNSIGNED) ? uint8_type_spec : int8_type_spec;
	}
	/*
	16bit integer
	signed short
	unsigned short
	signed short int
	unsigned short int
	*/
	else if (_is_type_spec(spec_flags, DECL_SPEC_SIGNED | DECL_SPEC_SHORT) ||
		_is_type_spec(spec_flags, DECL_SPEC_UNSIGNED | DECL_SPEC_SHORT) ||
		_is_type_spec(spec_flags, DECL_SPEC_SIGNED | DECL_SPEC_SHORT | DECL_SPEC_INT) ||
		_is_type_spec(spec_flags, DECL_SPEC_UNSIGNED | DECL_SPEC_SHORT | DECL_SPEC_INT))
	{
		return (spec_flags & DECL_SPEC_UNSIGNED) ? uint16_type_spec : int16_type_spec;
	}
	/*
	32bit integer
	signed int
	unsinged int
	signed
	unsigned
	signed long
	unsigned long
	signed long int
	unsigned long int
	*/
	else if (
		_is_type_spec(spec_flags, DECL_SPEC_SIGNED | DECL_SPEC_INT) ||
		_is_type_spec(spec_flags, DECL_SPEC_SIGNED) ||
		_is_type_spec(spec_flags, DECL_SPEC_SIGNED | DECL_SPEC_LONG) ||
		_is_type_spec(spec_flags, DECL_SPEC_SIGNED | DECL_SPEC_LONG | DECL_SPEC_INT) ||

		_is_type_spec(spec_flags, DECL_SPEC_UNSIGNED | DECL_SPEC_INT) ||
		_is_type_spec(spec_flags, DECL_SPEC_UNSIGNED) ||
		_is_type_spec(spec_flags, DECL_SPEC_UNSIGNED | DECL_SPEC_LONG) ||
		_is_type_spec(spec_flags, DECL_SPEC_UNSIGNED | DECL_SPEC_LONG | DECL_SPEC_INT))
	{
		return (spec_flags & DECL_SPEC_UNSIGNED) ? uint32_type_spec : int32_type_spec;
	}
	/*
	64bit integer
	signed long long
	unsigned long long
	signed long long int
	unsigned long long int
	*/
	else if (_is_type_spec(spec_flags, DECL_SPEC_SIGNED | DECL_SPEC_LONG | DECL_SPEC_LONG_LONG) ||
		_is_type_spec(spec_flags, DECL_SPEC_UNSIGNED | DECL_SPEC_LONG | DECL_SPEC_LONG_LONG) ||
		_is_type_spec(spec_flags, DECL_SPEC_SIGNED | DECL_SPEC_LONG | DECL_SPEC_LONG_LONG | DECL_SPEC_INT) ||
		_is_type_spec(spec_flags, DECL_SPEC_UNSIGNED | DECL_SPEC_LONG | DECL_SPEC_LONG_LONG | DECL_SPEC_INT))
	{
		return (spec_flags & DECL_SPEC_UNSIGNED) ? uint64_type_spec : int64_type_spec;
	}
	/*
	float
	signed float
	unsigned float
	*/
	else if (_is_type_spec(spec_flags, DECL_SPEC_SIGNED | DECL_SPEC_FLOAT) ||
		_is_type_spec(spec_flags, DECL_SPEC_UNSIGNED | DECL_SPEC_FLOAT))
	{
		return NULL;
	}
	/*
	double
	signed double
	unsigned double
	*/
	else if (_is_type_spec(spec_flags, DECL_SPEC_SIGNED | DECL_SPEC_DOUBLE) ||
		_is_type_spec(spec_flags, DECL_SPEC_UNSIGNED | DECL_SPEC_DOUBLE))
	{
		return NULL;
	}
	return NULL;
}

ast_type_spec_t* try_parse_type_spec(uint32_t* flag_result)
{
	uint32_t flags = 0;
	ast_user_type_spec_t* user_type = NULL;
	const char* alias = NULL;
	bool storage_class_specs = false;
	bool type_qualifiers = false;
	enum { tt_none, tt_user, tt_alias, tt_void, tt_int } type_type = tt_none;
	uint32_t num_spec_flags = 0;

	token_t* tok;

	while (!current_is(tok_eof))
	{
		tok = current();

		if (_is_user_type_specifier(current()))
		{
			//structs, untions, enums
			if (type_type != tt_none)
				return parse_err(ERR_SYNTAX, "multiple type specifiers are not permitted");
			user_type = parse_user_type();
			if (!user_type)
				return NULL;
			type_type = tt_user;
		}
		else if (current_is(tok_identifier) && _is_alias_name(tok->data.str))
		{
			//id -> occurs when the type has been typedef'd
			if (type_type != tt_none)
				break; //we've already seen int, struct, void etc
			alias = strdup(current()->data.str);
			type_type = tt_alias;
			next_tok();
		}
		else if (current_is(tok_void))
		{
			//void
			if (type_type != tt_none)
				return parse_err(ERR_SYNTAX, "multiple type specifiers are not permitted");
			type_type = tt_void;
			next_tok();
		}
		else if (_is_numeric_type_specifier(current()))
		{
			//we can consume multiple numeric type specifiers, eg unsigned long long int
			if (!(type_type == tt_none || type_type == tt_int))
				return parse_err(ERR_SYNTAX, "multiple type specifiers are not permitted");

			uint32_t flag = _get_decl_spec_flag(current());
			//special case for long long
			if (flag == DECL_SPEC_LONG && (num_spec_flags & DECL_SPEC_LONG))
				flag = DECL_SPEC_LONG_LONG;

			if (num_spec_flags & flag)
				return parse_err(ERR_SYNTAX, "duplicate %s specifier in numeric type", tok_kind_spelling(current()->kind));

			num_spec_flags |= flag;
			type_type = tt_int;

			next_tok();
		}
		else if (_is_storage_class_specifier(current()))
		{
			if (current_is(tok_inline))
			{
				flags |= TF_SC_INLINE;
			}
			else
			{
				//allow one of, extern, static, typedef, auto , register (auto and register are ignored)
				if (storage_class_specs)
					return parse_err(ERR_SYNTAX, "multiple storage class specifiers are not permitted");
				storage_class_specs = true;

				if (current_is(tok_extern))
					flags |= TF_SC_EXTERN;
				else if (current_is(tok_static))
					flags |= TF_SC_STATIC;
				else if (current_is(tok_typedef))
					flags |= TF_SC_TYPEDEF;
			}
			next_tok();
		}
		else if (_is_type_qualifier(current()))
		{
			type_qualifiers = true;
			if (current_is(tok_const))
			{
				//allow type qualifier const once only
				if (flags & TF_QUAL_CONST)
					return parse_err(ERR_SYNTAX, "multiple const specifiers are not permitted");
				flags |= TF_QUAL_CONST;
			}
			//ignore volatile
			next_tok();
		}
		else
		{
			break;
		}
	}

	if (type_type == tt_none)
	{
		if (type_qualifiers || storage_class_specs)
			return parse_err(ERR_SYNTAX, "incomplete type spec");
		return NULL;
	}
	ast_type_spec_t* type_spec = NULL;

	if (type_type == tt_int)
	{
		type_spec = _interpret_numeric_type_flags(num_spec_flags);
		if (!type_spec)
			return parse_err(ERR_SYNTAX, "failed to parse numeric type specification");
	}
	else if (type_type == tt_void)
	{
		type_spec = void_type_spec;
	}
	else if (type_type == tt_user)
	{
		type_spec = _alloc_type_spec();
		type_spec->kind = type_user;
		type_spec->data.user_type_spec = user_type;
		type_spec->size = 0; //size set at sema stage
	}
	else if (type_type == tt_alias)
	{
		type_spec = _alloc_type_spec();
		type_spec->kind = type_alias;
		type_spec->data.alias = alias;
		type_spec->size = 0;
	}
	else
	{
		assert(false);
	}
	*flag_result = flags;
	return type_spec;
}

parse_type_ref_result_t parse_type_ref(ast_type_spec_t* type_spec, uint32_t flags)
{
	parse_type_ref_result_t result;
	result.identifier = NULL;
	result.type = (ast_type_ref_t*)malloc(sizeof(ast_type_ref_t));
	memset(result.type, 0, sizeof(ast_type_ref_t));
	result.type->flags = flags;
	result.type->spec = type_spec;
	result.type->tokens.start = current();

	if (current_is(tok_l_paren) && next_is(tok_star))
	{
		//function pointer
		next_tok();
		next_tok();
		if (current_is(tok_identifier))
		{
			result.identifier = current();
			next_tok();
		}
		expect_cur(tok_r_paren);
		next_tok();

		//params
		ast_func_params_t* params = parse_function_parameters();
		result.type->spec = ast_make_func_ptr_type(result.type->spec, params);
	}
	else
	{
		while (current_is(tok_star))
		{
			//pointer
			next_tok();
			ast_type_spec_t* ptr_type = ast_make_ptr_type(result.type->spec);

			if (current_is(tok_const))
			{
				next_tok();
				//?
			}
			result.type->spec = ptr_type;
		}
	}
	
	result.type->tokens.end = current();
	return result;
}

ast_type_ref_t* try_parse_type_ref()
{
	uint32_t flags = 0;

	ast_type_spec_t* type_spec = try_parse_type_spec(&flags);
	if (!type_spec)
		return NULL;

	return parse_type_ref(type_spec, flags).type;
}

void parse_type_init()
{
	_alias_name_stack = _alloc_alias_name_set();
}