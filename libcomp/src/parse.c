#include "token.h"
#include "parse_internal.h"
#include "parse.h"
#include "diag.h"
#include "std_types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/*
see https://www.lysator.liu.se/c/ANSI-C-grammar-y.html
 
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

Note <declaration_specifiers> is incorrect for cast and size of - need to handle function pointers etc

<cast-exp> ::= <unary-exp>
				| "(" <declaration_specifiers> ")" <cast_expt>

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

token_t* _cur_tok = NULL;
static bool _parse_err = false;

void* parse_err(int err, const char* format, ...)
{
	char buff[512];

	va_list args;
	va_start(args, format);
	vsnprintf(buff, 512, format, args);
	va_end(args);

	_parse_err = true;
	diag_err(current(), err, buff);
	return NULL;
}

bool expect_cur(tok_kind k)
{
	if (!current_is(k))
	{
		parse_err(ERR_SYNTAX, "syntax error: expected '%s' before '%s'",
			tok_kind_spelling(k), tok_kind_spelling(current()->kind));
		return false;
	}
	return true;
}

void expect_next(tok_kind k)
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

static bool _is_assignment_op(token_t* tok)
{
	switch (tok->kind)
	{
	case tok_equal:	
	case tok_starequal:
	case tok_slashequal:
	case tok_percentequal:
	case tok_plusequal:
	case tok_minusequal:
	case tok_lesserlesserequal:
	case tok_greatergreaterequal:
	case tok_ampequal:
	case tok_carotequal:
	case tok_pipeequal:
		return true;
	}
	return false;
}

static op_kind _get_assignment_operator(token_t* tok)
{
	switch (tok->kind)
	{
	case tok_equal:
		return op_assign;
	case tok_starequal:
		return op_mul_assign;
	case tok_slashequal:
		return op_div_assign;
	case tok_percentequal:
		return op_mod_assign;
	case tok_plusequal:
		return op_add_assign;
	case tok_minusequal:
		return op_sub_assign;
	case tok_lesserlesserequal:
		return op_left_shift_assign;
	case tok_greatergreaterequal:
		return op_right_shift_assign;
	case tok_ampequal:
		return op_and_assign;
	case tok_carotequal:
		return op_xor_assign;
	case tok_pipeequal:
		return op_or_assign;
	}
	parse_err(ERR_SYNTAX, "Unknown assignment op %s", diag_tok_desc(tok));
	return op_unknown;
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
	parse_err(ERR_SYNTAX, "Unknown postfix op %s", diag_tok_desc(tok));
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
	parse_err(ERR_SYNTAX, "Unknown unary op %s", diag_tok_desc(tok));
	return op_unknown;
}

static op_kind _get_binary_operator(token_t* tok)
{
	switch (tok->kind)
	{
	case tok_equal:
		return op_assign;
	case tok_plusequal:
		return op_add_assign;

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
	parse_err(ERR_SYNTAX, "Unknown binary op %s", diag_tok_desc(tok));
	return op_unknown;
}

ast_expression_t* parse_alloc_expr()
{
	ast_expression_t* result = (ast_expression_t*)malloc(sizeof(ast_expression_t));
	memset(result, 0, sizeof(ast_expression_t));
	result->tokens.start = result->tokens.end = current();
	return result;
}

typedef ast_expression_t* (*bin_parse_fn)();

ast_expression_t* parse_binary_expression(tok_kind* op_set, bin_parse_fn sub_parse)
{
	token_t* start = current();
	ast_expression_t* expr = sub_parse();
	if (_parse_err || !expr)
	{
		ast_destroy_expression(expr);
		return NULL;
	}
	while (tok_in_set(current()->kind, op_set))
	{
		op_kind op = _get_binary_operator(current());
		next_tok();
		ast_expression_t* rhs_expr = sub_parse();

		if (!rhs_expr)
		{
			ast_destroy_expression(expr);
			return NULL;
		}

		//our two sides are now expr and rhs_expr
		//build a new binary op expression for expr
		ast_expression_t* cur_expr = expr;
		expr = parse_alloc_expr();
		expr->tokens.start = start;
		expr->kind = expr_binary_op;
		expr->data.binary_op.operation = op;
		expr->data.binary_op.lhs = cur_expr;
		expr->data.binary_op.rhs = rhs_expr;
	}
	if (_parse_err || !expr)
	{
		ast_destroy_expression(expr);
		return NULL;
	}
	expr->tokens.end = current();
	return expr;
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
	ast_expression_t* expr = parse_alloc_expr();
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
		ast_expression_t* expr = parse_alloc_expr();
		expr->kind = expr_int_literal;
		expr->data.int_literal.val = current()->data.int_val;
		expr->data.int_literal.type = NULL;
		next_tok();
		return expr;
	}
	else if (current_is(tok_string_literal))
	{
		ast_expression_t* expr = parse_alloc_expr();
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
		
		expr = try_parse_expression();

		if (!expr)
		{
			_cur_tok = start;
			return NULL;
		}

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
		ast_expression_t* expr = parse_alloc_expr();
		expr->tokens = primary->tokens;
		if (current_is(tok_l_paren))
		{
			if (primary->kind != expr_identifier)
			{
				parse_err(ERR_SYNTAX, "identifier must proceed function call");
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
				if (expr->data.func_call.first_param)
				{
					expect_cur(tok_comma);
					next_tok();
				}
				//ast_expression_t* param_expr = parse_expression();
				ast_func_call_param_t* param = (ast_func_call_param_t*)malloc(sizeof(ast_func_call_param_t));
				memset(param, 0, sizeof(ast_func_call_param_t));
				param->expr = parse_expression();

				if (!expr->data.func_call.first_param)
				{
					expr->data.func_call.first_param = expr->data.func_call.last_param = param;
				}
				else
				{
					param->prev = expr->data.func_call.last_param;
					expr->data.func_call.last_param->next = param;
					expr->data.func_call.last_param = param;
				}

				//param_expr->next = expr->data.func_call.params;
				//expr->data.func_call.params = param_expr;
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

ast_expression_t* try_parse_unary_expr();

/*
<cast-exp> ::= "(" <declaration_specifiers> ")" <cast_expt>
				| <unary-exp>

*/
ast_expression_t* try_parse_cast_expr()
{
	token_t* start = current();

	if (current_is(tok_l_paren))
	{
		next_tok();
		ast_type_ref_t* type = try_parse_type_ref();

		if (type)
		{
			expect_cur(tok_r_paren);
			next_tok();

			ast_expression_t* expr = parse_alloc_expr();
			expr->tokens.start = start;
			expr->tokens.end = current();
			expr->kind = expr_cast;
			expr->data.cast.type_ref = type;
			expr->data.cast.expr = try_parse_cast_expr();
			if (!expr->data.cast.expr)
			{
				parse_err(ERR_SYNTAX, "Expected expression after cast");
				ast_destroy_expression(expr);
				return NULL;
			}
			return expr;
		}
		_cur_tok = start;
	}

	return try_parse_unary_expr();
}

ast_expression_t* parse_sizeof_expr()
{
	ast_expression_t* expr = parse_alloc_expr();
	expr->kind = expr_sizeof;
	
	next_tok();

	if (current_is(tok_l_paren))
	{
		next_tok();
		//Could be a typespec, or an expression in parentheses
		ast_type_ref_t* type = try_parse_type_ref();
		
		if (type)
		{
			expr->data.sizeof_call.kind = sizeof_type;
			expr->data.sizeof_call.data.type_ref = type;
		}
		else
		{
			expr->data.sizeof_call.kind = sizeof_expr;
			expr->data.sizeof_call.data.expr = try_parse_unary_expr();
		}

		if (!expect_cur(tok_r_paren))
		{
			ast_destroy_expression(expr);
			return NULL;
		}
		next_tok();
	}
	else
	{
		expr->data.sizeof_call.kind = sizeof_expr;
		expr->data.sizeof_call.data.expr = try_parse_unary_expr();
	}
	expr->tokens.end = current();

	if (expr->data.sizeof_call.kind == sizeof_expr && !expr->data.sizeof_call.data.expr)
	{
		parse_err(ERR_SYNTAX, "failed to parse expression in sizeof()");
		//todo - return NULL?
	}	
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
		ast_expression_t* expr = parse_alloc_expr();
		expr->kind = expr_unary_op;
		expr->data.unary_op.operation = _get_unary_operator(current());
		next_tok();
		expr->data.unary_op.expression = try_parse_cast_expr();

		if (!expr->data.unary_op.expression)
		{
			parse_err(ERR_SYNTAX, "Expected expression after %s", ast_op_name(expr->data.unary_op.operation));
			ast_destroy_expression(expr);
			return NULL;
		}

		expr->tokens.end = current();
		return expr;
	}
	else if (current_is(tok_sizeof))
	{
		return parse_sizeof_expr();
	}
	return try_parse_postfix_expr();
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
	return parse_binary_expression(multiplacative_ops, try_parse_cast_expr);
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
		
		expr = parse_alloc_expr();
		expr->tokens.start = start;
		expr->kind = expr_condition;
		expr->data.condition.cond = condition;
		expr->data.condition.true_branch = parse_expression();
		expect_cur(tok_colon);
		next_tok();
		expr->data.condition.false_branch = parse_conditional_expression();
	}
	if (_parse_err || !expr)
	{
		ast_destroy_expression(expr);
		return NULL;
	}
	expr->tokens.end = current();
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

	if (expr && _is_assignment_op(current()))
	{
		op_kind op = _get_assignment_operator(current());
		next_tok();
		ast_expression_t* assignment = parse_alloc_expr();
		assignment->tokens = expr->tokens;
		assignment->kind = expr_binary_op;
		assignment->data.binary_op.operation = op;
		assignment->data.binary_op.lhs = expr;
		assignment->data.binary_op.rhs = parse_assignment_expression();
		assignment->tokens.end = current();
		return assignment;
	}
	else
	{
		_cur_tok = start;
		expr = parse_conditional_expression();
	}
	if (_parse_err || !expr)
	{
		ast_destroy_expression(expr);
		return NULL;
	}

	expr->tokens.end = current();
	return expr;
}

ast_expression_t* try_parse_expression()
{
	return parse_assignment_expression();
}

ast_expression_t* parse_expression()
{
	ast_expression_t* expr = parse_assignment_expression();
	if (!expr)
	{
		parse_err(ERR_SYNTAX, "failed to parse expression");
	}
	return expr;
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
		result->data.decl = decl;
	}
	else
	{
		result->kind = blk_smnt;
		result->data.smnt = parse_statement();
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
		parse_err(ERR_SYNTAX, "expected declaration, found %s", diag_tok_desc(current()));
	return result;
}

void parse_init(token_t* tok)
{
	types_init();
	parse_type_init();
	_cur_tok = tok;
	_parse_err = false;
	
}

ast_block_item_t* parse_block_list()
{
	ast_block_item_t* last = NULL;
	ast_block_item_t* result = NULL;

	parse_on_enter_block();

	while (!current_is(tok_r_brace))
	{
		ast_block_item_t* blk = parse_block_item();

		if (_parse_err)
			return NULL; //todo destroy

		if (last)
			last->next = blk;
		else
			result = blk;
		last = blk;
		last->next = NULL;
	}
	next_tok();

	parse_on_leave_block();

	return result;
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
			func->blocks = parse_block_list();
			decl->tokens.end = current();
		}
		else if(!found_semi)
		{
			parse_err(ERR_SYNTAX, "expected ';' after declaration of %s", ast_declaration_name(decl));
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