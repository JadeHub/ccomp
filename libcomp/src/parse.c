#include "token.h"
#include "parse.h"
#include "diag.h"
#include "std_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/*
<translation_unit> ::= { <function>
				| <declaration> }
<declaration> :: = <var_declaration> ";"
				| <type-specifier> ";"
				| <function_declaration> ";"
<var_declaration> ::= <ptr_decl_spec> <id> [ <array_decl> ] [= <exp>]
<array_decl> ::= "[" [ <constant-exp> ] "]"
<function_declaration> ::= <ptr_decl_spec> <id> "(" [ <function_param_list> ] ")"
<function_param_list> ::= <function_param_decl> { "," <function_param_decl> }
<function_param_decl> ::= <ptr_decl_spec> <id> [ <array_decl> ]
<function> ::= <function_declaration> "{" { <block-item> } "}"
<block-item> ::= [ <statement> | <declaration> ]
<statement> ::= "return" <exp> ";"
			  | <exp-option> ";"
			  | "if" "(" <exp> ")" <statement> [ "else" <statement> ]
			  | "{" { <block-item> } "}"
			  | "for" "(" <exp-option> ";" <exp-option> ";" <exp-option> ")" <statement>
			  | "for" "(" <declaration> <exp-option> ";" <exp-option> ")" <statement>
			  | "while" "(" <exp> ")" <statement>
			  | "do" <statement> "while" <exp> ";"
			  | "break" ";"
			  | "continue" ";"
			  | <switch-smnt>
			  | <var_declaration> ";"
			  | <label_smnt>

<label_smnt> ::= { <id> ":" } <statement>
<switch-smnt> ::= "switch" "(" <exp> ")" <switch_case> | { "{" <switch_case> "}" }
<switch_case> ::= "case" <constant-exp> ":" <statement> | "default" ":" <statement>
<exp> ::= <assignment-exp>
<assignment-exp> ::= <unary-exp> "=" <assignment-exp>
				| <conditional-exp>
<conditional-exp> ::= <logical-or-exp> "?" <exp> ":" <conditional-exp>
<constant-exp> ::= <conditional-exp>
<logical-or-exp> ::= <logical-and-exp> { "||" <logical-and-exp> }
<logical-and-exp> ::= <bitwise-or> { "&&" <bitwise-or> }
<bitwise-or> ::= <bitwise-xor> { ("|") <bitwise-xor> }
<bitwise-xor> ::= <bitwise-and> { ("^") <bitwise-and> }
<bitwise-and> ::= <equality-exp> { ("&") <equality-exp> }
<equality-exp> ::= <relational-exp> { ("!=" | "==") <relational-exp> }
<relational-exp> ::= <bitshift-exp> { ("<" | ">" | "<=" | ">=") <bitshift-exp> }
<bitshift-exp> ::= <additive-exp> { ("<<" | ">>") <additive-exp> }
<additive-exp> ::= <multiplacative-exp> { ("+" | "-") <multiplacative-exp> }
<multiplacative-exp> <cast-exp> { ("*" | "/") <cast-exp> }
<cast-exp> ::= <unary-exp>
<unary-exp> ::= <postfix-exp>
				| <unary-op> <cast-exp>
				| "sizeof" <unary-exp>
				| "sizeof" "(" <declaration_specifiers> ")"

<postfix-exp> ::= <primary-exp> {
				| "(" [ <exp> { "," <exp> } ] ")"
				| "." <id>
				| ("++" || "--")

				| "[" <exp> "]"
				| "->" <id> }
<primary-exp> ::= <id> | <literal> | "(" <exp> ")"

<literal> ::= <int>
			| "'" <char> "'"
			| <string_literal>

<unary_op> ::= "!" | "~" | "-" | "++" | "--" | "&" | "*"

<declaration_specifiers> ::= { ( <type_specifier> | <type_qualifier> | <storage_class_specifier> ) }
<ptr_decl_spec> ::= <declaration_specifiers> { "*" }

<type_qualifier> ::= "const"					//todo
					| "volatile"				//todo
<type_specifier> ::= "void"
					| "char"
					| "short"
					| "int"
					| "long"
					| "signed"					//todo
					| "unsigned"				//todo
					| "float"					//todo
					| "double"					//todo
					| <user_type_specifier>
					| <enum_specifier>
<storage_class_specifier> ::= "typedef"		//todo
							| "extern"			//todo
							| "static"			//todo
							| "auto"			//todo
							| "register"		//todo
<user_type_specifier> ::= ("struct" | "union") [ <id> ] [ "{" <struct_decl_list> "}" ]
<struct_decl_list> ::= <struct_decl> ["," <struct_decl_list> ]
<struct_decl> ::= <ptr_decl_spec> [ <id> ] [ ":" <int> ]
<enum_specifier> ::= "enum" [ <id> ] [ "{" { <id> [ = <int> ] } "}" ]
*/

static token_t* _cur_tok = NULL;
static bool _parse_err = false;

static inline token_t* current()
{
	return _cur_tok;
}

static inline token_t* next_tok()
{
	_cur_tok = _cur_tok->next;
	return _cur_tok;
}

static inline bool current_is(tok_kind k)
{
	return _cur_tok->kind == k;
}

static inline bool next_is(tok_kind k)
{
	return _cur_tok->next->kind == k;
}

static void report_err(int err, const char* format, ...)
{
	char buff[512];

	va_list args;
	va_start(args, format);
	vsnprintf(buff, 512, format, args);
	va_end(args);

	_parse_err = true;
	diag_err(current(), err, buff);
}

static bool expect_cur(tok_kind k)
{
	if (!current_is(k))
	{
		report_err(ERR_SYNTAX, "syntax error: expected '%s' before '%s'",
			tok_kind_spelling(k), tok_kind_spelling(current()->kind));
		return false;
	}
	return true;
}

static void expect_next(tok_kind k)
{
	next_tok();
	expect_cur(k);
}

static bool tok_in_set(tok_kind kind, tok_kind* set)
{
	while (*set != tok_invalid)
	{
		if (kind == *set)
			return true;
		set++;
	}
	return false;
}

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
	switch(tok->kind)
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
		return true;
	};
	return false;
}

static bool _is_postfix_unary_op(token_t* tok)
{
	switch (tok->kind)
	{
	case tok_plusplus:
	case tok_minusminus:
		return true;
	}
	return false;
}

static bool _is_unary_op(token_t* tok)
{
	switch (tok->kind)
	{
	case tok_minus:
	case tok_tilda:
	case tok_exclaim:
	case tok_plusplus:
	case tok_minusminus:
	case tok_amp:
	case tok_star:
		return true;
	}
	return false;
}

static op_kind _get_postfix_operator(token_t* tok)
{
	switch (tok->kind)
	{
	case tok_minusminus:
		return op_postfix_dec;
	case tok_plusplus:
		return op_postfix_inc;
	}
	report_err(ERR_SYNTAX, "Unknown postfix op %s", diag_tok_desc(tok));
	return op_unknown;
}

static op_kind _get_unary_operator(token_t* tok)
{
	switch (tok->kind)
	{
	case tok_minus:
		return op_negate;
	case tok_tilda:
		return op_compliment;
	case tok_exclaim:
		return op_not;
	case tok_minusminus:
		return op_prefix_dec;
	case tok_plusplus:
		return op_prefix_inc;
	case tok_amp:
		return op_address_of;
	case tok_star:
		return op_dereference;
	}
	report_err(ERR_SYNTAX, "Unknown unary op %s", diag_tok_desc(tok));
	return op_unknown;
}

static op_kind _get_binary_operator(token_t* tok)
{
	switch (tok->kind)
	{
	case tok_minus:
		return op_sub;
	case tok_plus:
		return op_add;
	case tok_star:
		return op_mul;
	case tok_slash:
		return op_div;
	case tok_ampamp:
		return op_and;
	case tok_pipepipe:
		return op_or;
	case tok_equalequal:
		return op_eq;
	case tok_exclaimequal:
		return op_neq;
	case tok_lesser:
		return op_lessthan;
	case tok_lesserequal:
		return op_lessthanequal;
	case tok_greater:
		return op_greaterthan;
	case tok_greaterequal:
		return op_greaterthanequal;
	case tok_lesserlesser:
		return op_shiftleft;
	case tok_greatergreater:
		return op_shiftright;
	case tok_amp:
		return op_bitwise_and;
	case tok_pipe:
		return op_bitwise_or;
	case tok_caret:
		return op_bitwise_xor;
	case tok_percent:
		return op_mod;
	}
	report_err(ERR_SYNTAX, "Unknown binary op %s", diag_tok_desc(tok));
	return op_unknown;
}

static ast_expression_t* _alloc_expr()
{
	ast_expression_t* result = (ast_expression_t*)malloc(sizeof(ast_expression_t));
	memset(result, 0, sizeof(ast_expression_t));
	result->tokens.start = result->tokens.end = current();
	return result;
}

static ast_type_ref_t* _make_type_ref(ast_type_spec_t* spec, token_t* start)
{
	ast_type_ref_t* result = (ast_type_ref_t*)malloc(sizeof(ast_type_ref_t));
	memset(result, 0, sizeof(ast_type_ref_t));
	result->spec = spec;
	result->tokens.start = start;
	result->tokens.end = current();
	return result;
}

typedef ast_expression_t* (*bin_parse_fn)();

ast_expression_t* parse_binary_expression(tok_kind* op_set, bin_parse_fn sub_parse)
{
	token_t* start = current();
	ast_expression_t* expr = sub_parse();
	if (_parse_err)
	{
		ast_destroy_expression(expr);
		return NULL;
	}
	while (tok_in_set(current()->kind, op_set))
	{
		op_kind op = _get_binary_operator(current());
		next_tok();
		ast_expression_t* rhs_expr = sub_parse();

		//our two sides are now expr and rhs_expr
		//build a new binary op expression for expr
		ast_expression_t* cur_expr = expr;
		expr = _alloc_expr();
		expr->tokens.start = start;
		expr->kind = expr_binary_op;
		expr->data.binary_op.operation = op;
		expr->data.binary_op.lhs = cur_expr;
		expr->data.binary_op.rhs = rhs_expr;
	}
	if (_parse_err)
	{
		ast_destroy_expression(expr);
		return NULL;
	}
	expr->tokens.end = current();
	return expr;
}

/*
<struct_decl> :: = <declaration_specifiers>[<id>][":" <int> ] ";"
*/
ast_struct_member_t* parse_struct_member()
{
	ast_struct_member_t* result = (ast_struct_member_t*)malloc(sizeof(ast_struct_member_t));
	memset(result, 0, sizeof(ast_struct_member_t));
	result->tokens.start = current();

	ast_type_spec_t* type = try_parse_pointer_decl_spec();
	if (!type)
	{
		report_err(ERR_SYNTAX, "expected declaration specification");
	}
	
	while (current_is(tok_star))
	{
		next_tok();
		//pointer
		type = ast_make_ptr_type(type);
	}

	result->type_ref = _make_type_ref(type, result->tokens.start);

	if (current_is(tok_identifier))
	{
		tok_spelling_cpy(current(), result->name, MAX_LITERAL_NAME);
		next_tok();
	}

	if (current_is(tok_colon))
	{
		next_tok();
		expect_cur(tok_num_literal);
		result->bit_size = int_val_as_uint32(&current()->data.int_val);
		next_tok();
	}

	expect_cur(tok_semi_colon);
	next_tok();
	result->tokens.end = current();
	return result;
}

/*
<enum_specifier> :: = "enum" [<id>] ["{" { <id> [= <int>] } "}"]
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
		
		int_val_t next_val = int_val_zero();
		while (true)
		{
			expect_cur(tok_identifier);

			ast_enum_member_t* member = (ast_enum_member_t*)malloc(sizeof(ast_enum_member_t));
			memset(member, 0, sizeof(ast_enum_member_t));
			member->tokens.start = current();
			tok_spelling_cpy(current(), member->name, MAX_LITERAL_NAME);
			next_tok();

			if (current_is(tok_equal))
			{
				next_tok();
				member->const_value = try_parse_literal();
				next_val = int_val_inc(member->const_value->data.int_literal.val);
			}
			else
			{
				member->const_value = _alloc_expr();
				member->const_value->tokens.start = member->tokens.start;
				member->const_value->tokens.end = current();
				member->const_value->kind = expr_int_literal;				
				member->const_value->data.int_literal.val = next_val;
				member->const_value->data.int_literal.type = NULL;
				next_val = int_val_inc(next_val);
			}

			member->next = result->enum_members;
			result->enum_members = member;

			member->tokens.end = current();

			if (!current_is(tok_comma))
				break;
			next_tok();
		}
		expect_cur(tok_r_brace);
		next_tok();
	}
	result->tokens.end = current();
	return result;
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

	if (current_is(tok_l_brace))
	{
		next_tok();
		uint32_t offset = 0;
		while (!current_is(tok_r_brace))
		{
			ast_struct_member_t* member = parse_struct_member();
			member->offset = offset;
			member->next = result->struct_members;
			result->struct_members = member;
			offset += member->type_ref->spec->size;
		}
		next_tok();
	}
	result->tokens.end = current();
	return result;
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

#define DECL_SPEC_TYPE_FLAGS	0x3FFF

//storage class specifiers
#define DECL_SPEC_EXTERN		1 << 20
#define DECL_SPEC_STATIC		1 << 21
#define DECL_SPEC_AUTO			1 << 22
#define DECL_SPEC_REGISTER		1 << 23

//type-qualifiers
#define DECL_SPEC_CONST			1 << 30
#define DECL_SPEC_VOLATILE		1 << 31

uint32_t _get_decl_spec_flag(token_t* tok)
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
	case tok_extern:
		return DECL_SPEC_EXTERN;
	case tok_static:
		return DECL_SPEC_STATIC;
	case tok_auto:
		return DECL_SPEC_AUTO;
	case tok_register:
		return DECL_SPEC_REGISTER;
	case tok_const:
		return DECL_SPEC_CONST;
	case tok_volatile:
		return DECL_SPEC_VOLATILE;
	}
	return 0;
}

static bool _is_type_spec(uint32_t flags, uint32_t type_flags)
{
	return ((flags & DECL_SPEC_TYPE_FLAGS) == type_flags);
}

static inline ast_type_spec_t* _make_type_spec()
{
	ast_type_spec_t* result = (ast_type_spec_t*)malloc(sizeof(ast_type_spec_t));
	memset(result, 0, sizeof(ast_type_spec_t));
	return result;
}

ast_type_spec_t* _make_decl_spec(token_t* start, token_t* end)
{
	uint32_t spec_flags = 0;

	token_t* tok = start;
	while (tok != end)
	{
		uint32_t flag = _get_decl_spec_flag(tok);

		//special case for long long
		if (flag == DECL_SPEC_LONG && (spec_flags & DECL_SPEC_LONG))
			flag = DECL_SPEC_LONG_LONG;

		if (spec_flags & flag)
			return NULL; //duplicate

		spec_flags |= flag;
		tok = tok->next;
	}

	//void
	if (_is_type_spec(spec_flags, DECL_SPEC_VOID))
	{
		return void_type_spec;
	}

	//struct and union
	if (_is_type_spec(spec_flags, DECL_SPEC_STRUCT) ||
		_is_type_spec(spec_flags, DECL_SPEC_UNION))
	{
		ast_type_spec_t* result = _make_type_spec();
		result->kind = type_user;
		result->user_type_spec = parse_struct_spec(spec_flags & DECL_SPEC_STRUCT ? user_type_struct : user_type_union);
		result->user_type_spec->tokens.start = start;
		result->size = ast_struct_size(result->user_type_spec);
		return result;
	}

	//enum
	if (_is_type_spec(spec_flags, DECL_SPEC_ENUM))
	{
		ast_type_spec_t* result = _make_type_spec();
		result->kind = type_user;
		result->user_type_spec = parse_enum_spec();
		result->user_type_spec->tokens.start = start;
		result->size = 4;
		return result;
	}

	//check for integer type

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
	else if (_is_type_spec(spec_flags, DECL_SPEC_SIGNED | DECL_SPEC_INT) ||
		_is_type_spec(spec_flags, DECL_SPEC_UNSIGNED | DECL_SPEC_INT) ||
		_is_type_spec(spec_flags, DECL_SPEC_SIGNED) ||
		_is_type_spec(spec_flags, DECL_SPEC_UNSIGNED) ||
		_is_type_spec(spec_flags, DECL_SPEC_SIGNED | DECL_SPEC_LONG) ||
		_is_type_spec(spec_flags, DECL_SPEC_UNSIGNED | DECL_SPEC_LONG) ||
		_is_type_spec(spec_flags, DECL_SPEC_SIGNED | DECL_SPEC_LONG | DECL_SPEC_INT) ||
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
		return NULL;
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

/*
<declaration_specifiers> ::= { ( <type_specifier> | <type_qualifier> | <storage_class_specifier> ) }
*/
ast_type_spec_t* try_parse_decl_spec()
{
	token_t* start = current();

	//consume all type_specifier, type_qualifier, storage_class_specifier
	uint32_t spec_flags = 0;
	while (_is_type_specifier(current()) || 
		_is_type_qualifier(current()) || 
		_is_storage_class_specifier(current()))
	{
		next_tok();
	}

	if (start != current())
	{
		ast_type_spec_t* result = _make_decl_spec(start, current());
		if (!result)
		{
			report_err(ERR_SYNTAX, "invalid combination of type specifiers");
			return NULL;
		}
		return result;
	}
	return NULL;
}

/*
<ptr_decl_spec> ::= <declaration_specifiers> { "*" }
*/
ast_type_spec_t* try_parse_pointer_decl_spec()
{
	ast_type_spec_t* type_spec = try_parse_decl_spec();
	if (!type_spec)
		return NULL;

	while (current_is(tok_star))
	{
		//pointer
		next_tok();
		ast_type_spec_t* ptr_type = ast_make_ptr_type(type_spec);
		type_spec = ptr_type;
	}
	return type_spec;
}

/*
<array_decl> ::= "[" [ <constant-exp> ] "]"

returns NULL if no array spec found
returns an expression of kind expr_null if empty array spec found '[]'
otherwise returns an expression representing the array size
*/
ast_expression_t* opt_parse_array_spec()
{
	ast_expression_t* result = NULL;
	if (current_is(tok_l_square_paren))
	{
		next_tok();

		if (!current_is(tok_r_square_paren))
		{
			result = parse_constant_expression();
		}
		else
		{
			result = _alloc_expr();
			result->kind = expr_null;
		}
		expect_cur(tok_r_square_paren);
		next_tok();
	}
	return result;
}

/*
<function_param_list> ::= <function_param_decl> { "," <function_param_decl> }
<function_param_decl> ::= <declaration_specifiers> { "*" } <id> [ <array_decl> ]
*/
void parse_function_parameters(ast_function_decl_t* func)
{
	ast_type_spec_t* type;

	token_t* start = current();
	while ((type = try_parse_pointer_decl_spec()))
	{
		ast_declaration_t* decl = (ast_declaration_t*)malloc(sizeof(ast_declaration_t));
		memset(decl, 0, sizeof(ast_declaration_t));
		decl->tokens.start = start;
		
		decl->kind = decl_var;
		decl->data.var.type_ref = _make_type_ref(type, start);
		
		ast_func_param_decl_t* param = (ast_func_param_decl_t*)malloc(sizeof(ast_func_param_decl_t));
		memset(param, 0, sizeof(ast_func_param_decl_t));
		param->decl = decl;		

		if (current_is(tok_identifier))
		{
			tok_spelling_cpy(current(), decl->data.var.name, MAX_LITERAL_NAME);
			next_tok();
		}

		// '[...]'
		decl->data.var.array_sz = opt_parse_array_spec();

		decl->tokens.end = current();

		if (!func->first_param)
		{
			func->first_param = func->last_param = param;
		}
		else
		{
			param->prev = func->last_param;
			func->last_param->next = param;
			func->last_param = param;
		}
		func->param_count++;

		if (!current_is(tok_comma))
			break;
		next_tok();

		start = current();
	}

	// if single void param remove it
	if (func->param_count == 1 && func->first_param->decl->data.var.type_ref->spec->kind == type_void)
	{
		free(func->first_param);
		func->first_param = func->last_param = NULL;
		func->param_count = 0;
	}
}

/*
<declaration> :: = <var_declaration> ";"
				| <declaration_specifiers> ";"
				| <function_declaration> ";"

<var_declaration> ::= <declaration_specifiers> <id> [= <exp>]
<function_declaration> ::= <declaration_specifiers> <id> "(" [ <function_params ] ")"
<declaration_specifiers> ::= { ( <type_specifier> | <type_qualifier> | <storage_class_specifiers> ) }
*/
ast_declaration_t* try_parse_declaration_opt_semi(bool* found_semi)
{
	token_t* start = current();

	ast_declaration_t* result = (ast_declaration_t*)malloc(sizeof(ast_declaration_t));
	memset(result, 0, sizeof(ast_declaration_t));
	result->tokens.start = start;
	result->tokens.end = current();
	
	ast_type_spec_t* type = try_parse_pointer_decl_spec();
	if (!type)
		return NULL;

	if (current_is(tok_identifier))
	{
		if (next_is(tok_l_paren))
		{
			//function			
			result->kind = decl_func;
			result->data.func.return_type_ref = _make_type_ref(type, start);
			tok_spelling_cpy(current(), result->data.func.name, MAX_LITERAL_NAME);
			next_tok();
			/*(*/
			expect_cur(tok_l_paren);
			next_tok();
			parse_function_parameters(&result->data.func);
			/*)*/
			expect_cur(tok_r_paren);
			next_tok();			
		}
		else
		{
			//variable 
			result->kind = decl_var;
			tok_spelling_cpy(current(), result->data.var.name, MAX_LITERAL_NAME);
			next_tok();
						
			result->data.var.type_ref = _make_type_ref(type, start);

			// '[...]'
			result->data.var.array_sz = opt_parse_array_spec();
						
			if (current_is(tok_equal))
			{
				next_tok();
				result->data.var.expr = parse_expression();
			}
		}
	}
	else
	{
		//type-spec
		result->kind = decl_type;
		result->data.type = type;
	}

	if (found_semi)
		*found_semi = current_is(tok_semi_colon);
	if (current_is(tok_semi_colon))
		next_tok();
	
	result->tokens.end = current();
	return result;
}

ast_declaration_t* try_parse_declaration()
{
	bool found_semi;
	ast_declaration_t* decl = try_parse_declaration_opt_semi(&found_semi);

	if (decl && !found_semi)
	{
		report_err(ERR_SYNTAX, "expected ';' after declaration of %s", ast_declaration_name(decl));
	}
	return decl;
}

/*
<declaration> :: = <var_declaration> | <function_declaration> | <type-specifier> ";"
*/
ast_declaration_t* parse_declaration()
{
	ast_declaration_t* result = try_parse_declaration();
	if (!result)
	{
		report_err(ERR_SYNTAX, "expected identifier");
	}
	return result;
}

/*
<identifier> ::= <id>
*/
ast_expression_t* parse_identifier()
{
	expect_cur(tok_identifier);
	if (_parse_err)
		return NULL;
	//<factor> ::= <id>
	ast_expression_t* expr = _alloc_expr();
	expr->kind = expr_identifier;
	tok_spelling_cpy(current(), expr->data.var_reference.name, MAX_LITERAL_NAME);
	next_tok();
	expr->tokens.end = current();
	return expr;
}

ast_expression_t* try_parse_literal()
{
	if (current_is(tok_num_literal))
	{
		//<factor> ::= <int>
		ast_expression_t* expr = _alloc_expr();
		expr->kind = expr_int_literal;
		expr->data.int_literal.val = current()->data.int_val;
		expr->data.int_literal.type = NULL;
		next_tok();
		return expr;
	}
	else if (current_is(tok_string_literal))
	{
		ast_expression_t* expr = _alloc_expr();
		expr->kind = expr_str_literal;
		expr->data.str_literal.value = current()->data.str;
		next_tok();
		return expr;
	}
	return NULL;
}

/*
<primary-exp> ::= <id> | <literal> | "(" <exp> ")"
*/
ast_expression_t* try_parse_primary_expr()
{
	ast_expression_t* expr = NULL;

	if (current_is(tok_identifier))
	{
		expr = parse_identifier();
	}
	else if (current_is(tok_l_paren))
	{
		token_t* start = current();
		next_tok();
		expr = parse_expression();
		expr->tokens.start = start; //otherwise we miss the initial '('
		expect_cur(tok_r_paren);
		next_tok();
	}
	else
	{
		expr = try_parse_literal();
	}
	if(expr)
		expr->tokens.end = current();
	return expr;
}

/*
<postfix-exp> ::= <primary-exp> {
				| "(" [ <exp> { "," <exp> } ] ")"
				| "." <id>
				| ("++" || "--")

				| "[" <exp> "]"
				| "->" <id> }
*/
static bool _is_postfix_op(token_t* tok)
{
	return tok->kind == tok_l_paren ||
		tok->kind == tok_fullstop ||
		tok->kind == tok_plusplus ||
		tok->kind == tok_minusminus;
}

ast_expression_t* try_parse_postfix_expr()
{
	static tok_kind postfix_ops[] = { tok_l_paren,
									tok_fullstop,
									tok_plusplus,
									tok_minusminus,
									tok_minusgreater,
									tok_l_square_paren,
									tok_invalid };

	ast_expression_t* primary = try_parse_primary_expr();

	if (!primary)
		return NULL;

	while (tok_in_set(current()->kind, postfix_ops))
	{
		ast_expression_t* expr = _alloc_expr();
		expr->tokens = primary->tokens;
		if (current_is(tok_l_paren))
		{
			if (primary->kind != expr_identifier)
			{
				report_err(ERR_SYNTAX, "identifier must proceed function call");
				ast_destroy_expression(primary);
				return NULL;
			}

			//function call
			expr->kind = expr_func_call;
			expr->data.func_call.target = primary;
			//todo - remove name and handle target as possible function ptr
			strcpy(expr->data.func_call.name, primary->data.var_reference.name);
			next_tok(); //skip tok_l_paren

			while (!current_is(tok_r_paren))
			{
				if (expr->data.func_call.params)
				{
					expect_cur(tok_comma);
					next_tok();
				}
				ast_expression_t* param_expr = parse_expression();
				param_expr->next = expr->data.func_call.params;
				expr->data.func_call.params = param_expr;
				expr->data.func_call.param_count++;
			}
			next_tok();
			expr->tokens.end = current();
		}
		else if (current_is(tok_fullstop))
		{
			//member access
			next_tok();
			expr->kind = expr_binary_op;
			expr->data.binary_op.lhs = primary;
			expr->data.binary_op.rhs = parse_identifier();
			expr->data.binary_op.operation = op_member_access;
			expr->tokens.end = current();
		}
		else if (current_is(tok_minusgreater))
		{
			//pointer member access
			next_tok();
			expr->kind = expr_binary_op;
			expr->data.binary_op.lhs = primary;
			expr->data.binary_op.rhs = parse_identifier();
			expr->data.binary_op.operation = op_ptr_member_access;
			expr->tokens.end = current();
		}
		else if (current_is(tok_plusplus) || current_is(tok_minusminus))
		{
			//postfix inc/dec
			expr->kind = expr_postfix_op;
			expr->data.unary_op.operation = _get_postfix_operator(current());
			expr->data.unary_op.expression = primary;
			next_tok();
			expr->tokens.end = current();
		}
		else if (current_is(tok_l_square_paren))
		{
			//pointer member access
			next_tok();
			expr->kind = expr_binary_op;
			expr->data.binary_op.lhs = primary;
			expr->data.binary_op.rhs = parse_expression();
			expr->data.binary_op.operation = op_array_subscript;
			expect_cur(tok_r_square_paren);
			next_tok();			
			expr->tokens.end = current();
		}
		primary = expr;
	}
	return primary;
}

ast_expression_t* parse_cast_expr();

ast_expression_t* parse_sizeof_expr()
{
	ast_expression_t* expr = _alloc_expr();
	expr->kind = expr_sizeof;
	
	next_tok();

	if (current_is(tok_l_paren))
	{
		next_tok();
		//Could be a typespec, or an expression in parentheses
		ast_type_spec_t* type = try_parse_decl_spec();
		
		if (type)
		{
			expr->data.sizeof_call.kind = sizeof_type;
			expr->data.sizeof_call.type = type;
		}
		else
		{
			expr->data.sizeof_call.kind = sizeof_expr;
			expr->data.unary_op.expression = parse_unary_expr();
		}

		expect_cur(tok_r_paren);
		next_tok();
	}
	else
	{
		expr->data.sizeof_call.kind = sizeof_expr;
		expr->data.unary_op.expression = parse_unary_expr();
	}
	
	expr->tokens.end = current();
	return expr;
}

/*
<unary-exp> ::= <postfix-exp>
				| <unary-op> <cast-exp>
				| "sizeof" <unary-exp>
				| "sizeof" "(" <declaration_specifiers> ")"
*/
ast_expression_t* try_parse_unary_expr()
{
	if (_is_unary_op(current()))
	{
		ast_expression_t* expr = _alloc_expr();
		expr->kind = expr_unary_op;
		expr->data.unary_op.operation = _get_unary_operator(current());
		next_tok();
		expr->data.unary_op.expression = parse_cast_expr();
		expr->tokens.end = current();
		return expr;
	}
	else if (current_is(tok_sizeof))
	{
		return parse_sizeof_expr();
	}
	return try_parse_postfix_expr();
}

ast_expression_t* parse_unary_expr()
{
	ast_expression_t* expr = try_parse_unary_expr();
	if (!expr)
		report_err(ERR_SYNTAX, "failed to parse expression");
	return expr;
}

/*
<cast-exp> ::= <unary-exp>
*/
ast_expression_t* parse_cast_expr()
{
	return parse_unary_expr();
}

static tok_kind logical_or_ops[] = { tok_pipepipe, tok_invalid };
static tok_kind logical_and_ops[] = { tok_ampamp, tok_invalid };
static tok_kind bitwise_or_ops[] = { tok_pipe, tok_invalid };
static tok_kind bitwise_xor_ops[] = { tok_caret, tok_invalid };
static tok_kind bitwise_and_ops[] = { tok_amp, tok_invalid };
static tok_kind equality_ops[] = { tok_equalequal, tok_exclaimequal, tok_invalid };
static tok_kind relational_ops[] = { tok_lesser, tok_lesserequal, tok_greater, tok_greaterequal, tok_invalid };
static tok_kind bitshift_ops[] = { tok_lesserlesser, tok_greatergreater, tok_invalid };
static tok_kind additive_ops[] = { tok_plus, tok_minus, tok_invalid };
static tok_kind multiplacative_ops[] = { tok_star, tok_slash, tok_percent, tok_invalid };

/*
<multiplacative-exp> <cast-exp> { ("*" | "/") <cast-exp> }
*/
ast_expression_t* parse_multiplacative_expr()
{
	return parse_binary_expression(multiplacative_ops, parse_cast_expr);
 }

/*
<additive-exp> ::= <term> { ("+" | "-") <term> }
*/
ast_expression_t* parse_additive_expr()
{
	return parse_binary_expression(additive_ops, parse_multiplacative_expr);
}

/*
<bitshift - exp> :: = <additive - exp>{ ("<<" | ">>") < additive - exp > }
*/
ast_expression_t* parse_bitshift_expr()
{
	return parse_binary_expression(bitshift_ops, parse_additive_expr);
}

/*
<relational - exp> :: = <additive - exp>{ ("<" | ">" | "<=" | ">=") < additive - exp > }
*/
ast_expression_t* parse_relational_expr()
{
	return parse_binary_expression(relational_ops, parse_bitshift_expr);
}

/*
<equality-exp> :: = <relational - exp>{ ("!=" | "==") < relational - exp > }
*/
ast_expression_t* parse_equality_expr()
{
	return parse_binary_expression(equality_ops, parse_relational_expr);
}

/*
<bitwise-and> :: = <equality-exp>{ ("&") < equality-exp > }
*/
ast_expression_t* parse_bitwise_and_expr()
{
	return parse_binary_expression(bitwise_and_ops, parse_equality_expr);
}

/*
<bitwise-xor> :: = <bitwise-and>{ ("^") < bitwise-and > }
*/
ast_expression_t* parse_bitwise_xor_expr()
{
	return parse_binary_expression(bitwise_xor_ops, parse_bitwise_and_expr);
}

/*
<bitwise-or > :: = <bitwise-xor>{ ("|") < bitwise-xor > }
*/
ast_expression_t* parse_bitwise_or_expr()
{
	return parse_binary_expression(bitwise_or_ops, parse_bitwise_xor_expr);
}

/*
<logical-and-exp> ::= <bitwise-or> { "&&" <bitwise-or> }
*/
ast_expression_t* parse_logical_and_expr()
{
	return parse_binary_expression(logical_and_ops, parse_bitwise_or_expr);
}

/*
<logical-or-exp> :: = <logical-and-exp>{ "||" <logical-and-exp > }
*/
ast_expression_t* parse_logical_or_expr()
{
	return parse_binary_expression(logical_or_ops, parse_logical_and_expr);
}

/*
<conditional-exp> :: = <logical-or-exp> "?" <exp> ":" <conditional-exp>
*/
ast_expression_t* parse_conditional_expression()
{
	token_t* start = current();
	ast_expression_t* expr = parse_logical_or_expr();

	if (current_is(tok_question))
	{
		next_tok();
		ast_expression_t* condition = expr;
		
		expr = _alloc_expr();
		expr->tokens.start = start;
		expr->kind = expr_condition;
		expr->data.condition.cond = condition;
		expr->data.condition.true_branch = parse_expression();
		expect_cur(tok_colon);
		next_tok();
		expr->data.condition.false_branch = parse_conditional_expression();
	}
	if (_parse_err)
	{
		ast_destroy_expression(expr);
		return NULL;
	}
	return expr;
}

/*
<constant-exp> :: = <conditional-exp>
*/
ast_expression_t* parse_constant_expression()
{
	return parse_conditional_expression();
}

/*
<assignment-exp> ::= <id> "=" <exp> | <conditional-or-exp>
<assignment-exp> ::= <unary-exp> "=" <assignment-exp>
				| <conditional-exp>
*/
ast_expression_t* parse_assignment_expression()
{
	ast_expression_t* expr;
	token_t* start = _cur_tok;

	expr = try_parse_unary_expr();

	if (expr && current_is(tok_equal))
	{
		next_tok();
		ast_expression_t* assignment = _alloc_expr();
		assignment->tokens = expr->tokens;
		assignment->kind = expr_assign;
		assignment->data.assignment.target = expr;		
		assignment->data.assignment.expr = parse_assignment_expression();
		assignment->tokens.end = current();
		return assignment;
	}
	else
	{
		_cur_tok = start;
		expr = parse_conditional_expression();
	}
	if (_parse_err)
	{
		ast_destroy_expression(expr);
		return NULL;
	}

	expr->tokens.end = current();
	return expr;
}

ast_expression_t* parse_expression()
{
	return parse_assignment_expression();
}

/*
<exp-option> ::= ";" | <expression>

Note: the following is used to parse the 'step' expression in a for loop
<exp-option> ::= ")" | <expression>
*/
ast_expression_t* parse_optional_expression(tok_kind term_tok)
{

	ast_expression_t* result = NULL;

	if (current_is(term_tok))
	{
		result = _alloc_expr();
		result->kind = expr_null;
		return result;
	}
	result = parse_expression();
	if (_parse_err)
	{
		ast_destroy_expression(result);
		return NULL;
	}
	expect_cur(term_tok);
	return result;
}

/*
<switch_case> ::= "case" <constant-exp> ":" <statement> | "default" ":" <statement>
*/
ast_switch_case_data_t* parse_switch_case()
{
	bool def = current_is(tok_default);
	next_tok();

	ast_switch_case_data_t* result = (ast_switch_case_data_t*)malloc(sizeof(ast_switch_case_data_t));
	memset(result, 0, sizeof(ast_switch_case_data_t));

	if (!def)
	{
		result->const_expr = parse_constant_expression();
	}

	expect_cur(tok_colon);
	next_tok();

	if (_parse_err)
		return NULL;
	
	//statement is optional if this is not the default case
	if (!current_is(tok_case) && !current_is(tok_default) && !current_is(tok_r_brace))
		result->statement = parse_statement();
	return result;
}

/*
<statement> ::= "return" <exp> ";"
			  | <exp-option> ";"
			  | "if" "(" <exp> ")" <statement> [ "else" <statement> ]
			  | "{" { <block-item> } "}
			  | "for" "(" <exp-option> ";" <exp-option> ";" <exp-option> ")" <statement>
			  | "for" "(" <declaration> <exp-option> ";" <exp-option> ")" <statement>
			  | "while" "(" <exp> ")" <statement>
			  | "do" <statement> "while" <exp> ";"
			  | "break" ";"
			  | "continue" ";"
			  | <switch_smnt>
			  | <label_smnt>
*/
ast_statement_t* parse_statement()
{
	ast_statement_t* result = (ast_statement_t*)malloc(sizeof(ast_statement_t));
	memset(result, 0, sizeof(ast_statement_t));
	result->tokens.start = current();
	if (current_is(tok_return))
	{
		//<statement> ::= "return" < exp > ";"
		next_tok();
		result->kind = smnt_return;
		result->data.expr = parse_optional_expression(tok_semi_colon);// parse_expression();
		expect_cur(tok_semi_colon);
		next_tok();
	}
	else if (current_is(tok_continue))
	{
		//<statement> ::= "continue" ";"
		next_tok();
		result->kind = smnt_continue;
		expect_cur(tok_semi_colon);
		next_tok();
	}
	else if (current_is(tok_break))
	{
		//<statement> ::= "break" ";"
		next_tok();
		result->kind = smnt_break;
		expect_cur(tok_semi_colon);
		next_tok();
	}
	else if (current_is(tok_for))
	{
		expect_next(tok_l_paren);
		next_tok();

		//<statement> ::= "for" "(" <declaration> <exp-option> ";" <exp-option> ")" <statement>
		//<statement> ::= "for" "(" <exp-option> ";" <exp-option> ";" <exp-option> ")" <statement>
		result->data.for_smnt.init_decl = try_parse_declaration();
		result->kind = smnt_for_decl;
		if (!result->data.for_smnt.init_decl)
		{
			result->kind = smnt_for;
			result->data.for_smnt.init = parse_optional_expression(tok_semi_colon);
			expect_cur(tok_semi_colon);
			next_tok();
		}
		result->data.for_smnt.condition = parse_optional_expression(tok_semi_colon);
		expect_cur(tok_semi_colon);
		next_tok();
		result->data.for_smnt.post = parse_optional_expression(tok_r_paren);
		expect_cur(tok_r_paren);
		next_tok();
		result->data.for_smnt.statement = parse_statement();

		if (result->data.for_smnt.condition->kind == expr_null)
		{
			//if no condition add a constant literal of 1
			result->data.for_smnt.condition->kind = expr_int_literal;
			result->data.for_smnt.condition->data.int_literal.val = int_val_one();
		}
	}
	else if (current_is(tok_while))
	{
		//<statement> :: = "while" "(" <exp> ")" <statement> ";"
		expect_next(tok_l_paren);
		next_tok();
		result->kind = smnt_while;
		result->data.while_smnt.condition = parse_expression();
		expect_cur(tok_r_paren);
		next_tok();
		result->data.while_smnt.statement = parse_statement();
	}
	else if (current_is(tok_do))
	{
		next_tok();
		//<statement> :: = "do" <statement> "while" <exp> ";"
		result->kind = smnt_do;
		result->data.while_smnt.statement = parse_statement();
		expect_cur(tok_while);
		next_tok();
		result->data.while_smnt.condition = parse_expression();
		expect_cur(tok_semi_colon);
		next_tok();
	}
	else if (current_is(tok_if))
	{
		//<statement> ::= "if" "(" <exp> ")" <statement> [ "else" <statement> ]
		expect_next(tok_l_paren);
		next_tok();
		result->kind = smnt_if;
		result->data.if_smnt.condition = parse_expression();
		expect_cur(tok_r_paren);
		next_tok();
		result->data.if_smnt.true_branch = parse_statement();

		if (current_is(tok_else))
		{
			next_tok();
			result->data.if_smnt.false_branch = parse_statement();
		}
	}
	else if (current_is(tok_l_brace))
	{
		//<statement> ::= "{" { <block - item> } "}"
		next_tok();
		result->kind = smnt_compound;
		ast_block_item_t* block;
		ast_block_item_t* last_block = NULL;
		while (!current_is(tok_r_brace))
		{
			block = parse_block_item();

			if (last_block)
				last_block->next = block;
			else
				result->data.compound.blocks = block;
			last_block = block;
		}
		next_tok();
	}
	else if (current_is(tok_switch))
	{
		next_tok();
		result->kind = smnt_switch;

		expect_cur(tok_l_paren);
		next_tok();
		result->data.switch_smnt.expr = parse_expression();
		expect_cur(tok_r_paren);
		next_tok();

		if (current_is(tok_l_brace))
		{
			next_tok();

			ast_switch_case_data_t* last_case = NULL;
			while (current_is(tok_case) || current_is(tok_default))
			{
				bool def = current_is(tok_default);
				ast_switch_case_data_t* case_data = parse_switch_case();

				if (!case_data)
				{
					goto parse_err;
				}

				if (def)
				{
					if (result->data.switch_smnt.default_case)
					{
						report_err(ERR_SYNTAX, "Multiple default cases in switch statement");
						goto parse_err;
					}
					result->data.switch_smnt.default_case = case_data;
				}
				else
				{
					//maintain list order
					if (last_case == NULL)
						result->data.switch_smnt.cases = case_data;
					else
						last_case->next = case_data;
					last_case = case_data;
					result->data.switch_smnt.case_count++;
				}

				if (current_is(tok_r_brace))
				{
					next_tok();
					break;
				}
			}
		}
		else if (current_is(tok_case) || current_is(tok_default))
		{
			parse_switch_case(&result->data.switch_smnt);
		}
	}
	else if (current_is(tok_identifier) && next_is(tok_colon))
	{		
		//statement ::= <label_smnt>
		result->kind = smnt_label;
		tok_spelling_cpy(current(), result->data.label_smnt.label, MAX_LITERAL_NAME);
		next_tok(); //identifier
		next_tok(); //colon
		result->data.label_smnt.smnt = parse_statement();
	}
	else if (current_is(tok_goto))
	{
		next_tok();
		expect_cur(tok_identifier);
		result->kind = smnt_goto;
		tok_spelling_cpy(current(), result->data.goto_smnt.label, MAX_LITERAL_NAME);
		next_tok();
	}
	else
	{
		//<statement ::= <exp-option> ";"
		result->kind = smnt_expr;
		result->data.expr = parse_optional_expression(tok_semi_colon);
		expect_cur(tok_semi_colon);
		next_tok();
	}
	if (!_parse_err)
	{
		result->tokens.end = current();
		return result;
	}
parse_err:
	ast_destroy_statement(result);
	return NULL;
}

/*
<block-item> ::= <statement> | <declaration>
*/
ast_block_item_t* parse_block_item()
{
	ast_block_item_t* result = (ast_block_item_t*)malloc(sizeof(ast_block_item_t));
	memset(result, 0, sizeof(ast_block_item_t));
	result->tokens.start = current();

	ast_declaration_t* decl = try_parse_declaration();
	if (decl)
	{
		result->kind = blk_decl;
		result->decl = decl;
	}
	else
	{
		result->kind = blk_smnt;
		result->smnt = parse_statement();
	}
	if (_parse_err)
		return NULL;
	result->tokens.end = current();
	return result;
}

static void _add_decl_to_tl(ast_trans_unit_t* tl, ast_declaration_t* decl)
{
	decl->next = NULL;
	ast_declaration_t* tmp = tl->decls;

	if (!tmp)
	{
		tl->decls = decl;
		return;
	}

	while (tmp->next)
	{
		tmp = tmp->next;
	}
	tmp->next = decl;
}

ast_declaration_t* parse_top_level_decl(bool* found_semi)
{
	ast_declaration_t* result = try_parse_declaration_opt_semi(found_semi);
	if (!result && !_parse_err)
		report_err(ERR_SYNTAX, "expected declaration, found %s", diag_tok_desc(current()));
	return result;
}

void parse_init(token_t* tok)
{
	types_init();
	_cur_tok = tok;
	_parse_err = false;
}

//<translation_unit> :: = { <function> | <declaration> }
ast_trans_unit_t* parse_translation_unit()
{
	ast_trans_unit_t* result = (ast_trans_unit_t*)malloc(sizeof(ast_trans_unit_t));
	memset(result, 0, sizeof(ast_trans_unit_t));
	result->tokens.start = current();
	
	while (!current_is(tok_eof))
	{
		bool found_semi;
		ast_declaration_t* decl = parse_top_level_decl(&found_semi);
		if (_parse_err)
			goto parse_failure;

		if (decl->kind == decl_func && !found_semi)
		{
			expect_cur(tok_l_brace);
			next_tok();
			//function definition
			ast_function_decl_t* func = &decl->data.func;

			ast_block_item_t* last_blk = NULL;
			while (!current_is(tok_r_brace))
			{
				ast_block_item_t* blk = parse_block_item();

				if (_parse_err)
					goto parse_failure;
				
				if (last_blk)
					last_blk->next = blk;
				else
					func->blocks = blk;
				last_blk = blk;
				last_blk->next = NULL;
			}
			next_tok();
			decl->tokens.end = current();
		}
		else if(!found_semi)
		{
			report_err(ERR_SYNTAX, "expected ';' after declaration of %s", ast_declaration_name(decl));
		}
		if (_parse_err)
			goto parse_failure;
		_add_decl_to_tl(result, decl);		
	}

	result->tokens.end = current();
	expect_cur(tok_eof);
	return result;

parse_failure:
	ast_destory_translation_unit(result);
	return NULL;
}