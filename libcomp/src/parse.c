#include "token.h"
#include "parse.h"
#include "diag.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

ast_expression_t* parse_expression();
ast_block_item_t* parse_block_item();
ast_type_spec_t* try_parse_type_spec();

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
		return true;
	}
	return false;
}

static bool _is_builtin_type(token_t* tok)
{
	return tok->kind == tok_int ||
		tok->kind == tok_void;
}

static uint32_t _get_builtin_type_size(token_t* tok)
{
	if (tok->kind == tok_int)
		return 4;
	return 0;
}

static type_kind _get_builtin_type_kind(token_t* tok)
{
	switch (tok->kind)
	{
	case tok_int:
		return type_int;
	case tok_void:
		return type_void;
	}
	report_err(ERR_SYNTAX, "unknown type %s", tok_kind_spelling(tok->kind));
	return type_void;
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
	report_err(ERR_SYNTAX, "Unknown postfix op %s", tok_kind_spelling(tok->kind));
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
	}
	report_err(ERR_SYNTAX, "Unknown unary op %s", tok_kind_spelling(tok->kind));
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
	report_err(ERR_SYNTAX, "Unknown binary op %s", tok_kind_spelling(tok->kind));
	return op_unknown;
}

static ast_expression_t* _alloc_expr()
{
	ast_expression_t* result = (ast_expression_t*)malloc(sizeof(ast_expression_t));
	memset(result, 0, sizeof(ast_expression_t));
	result->tokens.start = result->tokens.end = current();
	return result;
}

/*
<translation_unit> ::= { <function> 
				| <declaration> }
<declaration> :: = <var_declaration> ";"
				| <type-specifier> ";"
				| <function_declaration> ";"
<var_declaration> ::= <type_specifier> <id> [=<exp>]
<function_declaration> ::= <type_specifier> <id> "(" [ <type_specifier> <id> { "," <type_specifier> <id> } ] ")"
<function> ::= <function_declaration> "{" { <block-item> } "}"
<block-item> ::= [ <statement> | <declaration> ]
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
			  | <var_declaration> ";"
<exp> ::= <assignment-exp>
<assignment-exp> ::= <unary-exp> "=" <assignment-exp>
				| <conditional-exp>
<conditional-exp> ::= <logical-or-exp> "?" <exp> ":" <conditional-exp>
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
<unary-exp> ::= <postfix_op>
				| <unary-op> <cast-exp>

<postfix-exp> ::= <primary-exp> {
				| "(" [ <exp> { "," <exp> } ] ")"
				| "." <id>
				| ("++" || "--")
				
				| "[" <exp> "]"
				| "->" <id> }
<primary-exp> ::= <id> | <literal> | "(" <exp> ")"

<literal> ::= <int>

<unary_op> ::= "!" | "~" | "-" | "++" | "--"
<type_specifier> ::= "int"
				| <struct_specifier>
<struct_specifier> ::= ("struct" | "union") [ <id> ] [ "{" <struct_decl_list> "}" ]
<struct_decl_list> ::= <struct_decl> ["," <struct_decl_list> ]
<struct_decl> ::= <type_specifier> [ <id> ] [ ":" <int> ]
*/

tok_kind logical_or_ops[] = { tok_pipepipe, tok_invalid };
tok_kind logical_and_ops[] = { tok_ampamp, tok_invalid };
tok_kind bitwise_or_ops[] = { tok_pipe, tok_invalid };
tok_kind bitwise_xor_ops[] = { tok_caret, tok_invalid };
tok_kind bitwise_and_ops[] = { tok_amp, tok_invalid };
tok_kind equality_ops[] = { tok_equalequal, tok_exclaimequal, tok_invalid };
tok_kind relational_ops[] = { tok_lesser, tok_lesserequal, tok_greater, tok_greaterequal, tok_invalid };
tok_kind bitshift_ops[] = { tok_lesserlesser, tok_greatergreater, tok_invalid };
tok_kind additive_ops[] = { tok_plus, tok_minus, tok_invalid };
tok_kind multiplacative_ops[] = { tok_star, tok_slash, tok_percent, tok_invalid };

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

typedef ast_expression_t* (*bin_parse_fn)();

ast_expression_t* parse_binary_expression(tok_kind* op_set, bin_parse_fn sub_parse)
{
	token_t* start = current();
	ast_expression_t* expr = sub_parse();
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
<struct_decl> :: = <type_specifier>[<id>][":" <int> ] ";"
*/
ast_struct_member_t* parse_struct_member()
{
	ast_struct_member_t* result = (ast_struct_member_t*)malloc(sizeof(ast_struct_member_t));
	memset(result, 0, sizeof(ast_struct_member_t));
	result->tokens.start = current();
	result->type = try_parse_type_spec();
	if (!result->type)
	{
		report_err(ERR_SYNTAX, "expected type specification");
	}

	if (current_is(tok_identifier))
	{
		tok_spelling_cpy(current(), result->name, MAX_LITERAL_NAME);
		next_tok();
	}

	if (current_is(tok_colon))
	{
		next_tok();
		expect_cur(tok_num_literal);
		result->bit_size = (uint32_t)current()->data;
		next_tok();
	}

	expect_cur(tok_semi_colon);
	next_tok();
	result->tokens.end = current();
	return result;
}

/*
<struct_specifier> :: = ("struct" | "union") [<id>] ["{" < struct_decl_list > "}"]
*/
ast_struct_spec_t* parse_struct_spec()
{
	ast_struct_spec_t* result = (ast_struct_spec_t*)malloc(sizeof(ast_struct_spec_t));
	memset(result, 0, sizeof(ast_struct_spec_t));
	result->kind = current_is(tok_struct) ? struct_struct : struct_union;
	next_tok();

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
			member->next = result->members;
			result->members = member;
			offset += member->type->size;
		}
		next_tok();
	}

	return result;
}

/*
<type_specifier> :: = "int" | <struct_specifier>
*/
ast_type_spec_t* try_parse_type_spec()
{
	token_t* start = current();
	if(_is_builtin_type(current()))
	{
		ast_type_spec_t* result = (ast_type_spec_t*)malloc(sizeof(ast_type_spec_t));
		memset(result, 0, sizeof(ast_type_spec_t));
		result->tokens.start = current();
		result->kind = _get_builtin_type_kind(current());
		result->size = _get_builtin_type_size(current());
		next_tok();
		result->tokens.end = current();
		return result;
	}
	else if (current_is(tok_struct) || current_is(tok_union))
	{
		ast_type_spec_t* result = (ast_type_spec_t*)malloc(sizeof(ast_type_spec_t));
		memset(result, 0, sizeof(ast_type_spec_t));
		result->tokens.start = current();
		result->kind = type_struct;
		result->struct_spec = parse_struct_spec();
		ast_struct_member_t* member = result->struct_spec->members;
		while (member)
		{
			result->size += member->type->size;
			member = member->next;
		}
		result->tokens.end = current();
		return result;
	}
	_cur_tok = start;
	return NULL;
}

/*
<function_params> ::= [ <type-specifier> [<id>] { "," <type-specifier> [<id>] } ]
*/
void parse_function_parameters(ast_function_decl_t* func)
{
	ast_type_spec_t* type;
	ast_declaration_t* last_param = NULL;

	while ((type = try_parse_type_spec()))
	{
		ast_declaration_t* param = (ast_declaration_t*)malloc(sizeof(ast_declaration_t));
		memset(param, 0, sizeof(ast_declaration_t));
		param->tokens.start = current();
		param->kind = decl_var;
		param->data.var.type = type;

		if (current_is(tok_identifier))
		{
			tok_spelling_cpy(current(), param->data.var.name, MAX_LITERAL_NAME);
			next_tok();
		}
		param->tokens.end = current();
		if (last_param)
			last_param->next = param;
		else
			func->params = param;
		last_param = param;
		func->param_count++;

		if (!current_is(tok_comma))
			break;
		next_tok();


		/*ast_function_param_t* param = (ast_function_param_t*)malloc(sizeof(ast_function_param_t));
		memset(param, 0, sizeof(ast_function_param_t));
		param->tokens.start = current();
		param->type = type;
		if (current_is(tok_identifier))
		{
			tok_spelling_cpy(current(), param->name, MAX_LITERAL_NAME);
			next_tok();
		}
		param->tokens.end = current();
		if (last_param)
			last_param->next = param;
		else
			func->params = param;
		last_param = param;
		func->param_count++;
		
		if (!current_is(tok_comma))
			break;
		next_tok();*/
	}

	// if single void param remove it
	if (func->param_count == 1 && func->params->data.var.type->kind == type_void)
	{
		free(func->params);
		func->params = NULL;
		func->param_count = 0;
	}
}

/*
<var_declaration> ::= <type_specifier> <id> [=<exp>] ";"
*/
ast_declaration_t* parse_var_decl()
{
	ast_declaration_t* result = (ast_declaration_t*)malloc(sizeof(ast_declaration_t));
	memset(result, 0, sizeof(ast_declaration_t));
	result->tokens.start = current();
	result->kind = decl_var;

	result->data.var.type = try_parse_type_spec();
	if (!result->data.var.type)
	{
		report_err(ERR_SYNTAX, "expected type spec");
	}

	expect_cur(tok_identifier);
	tok_spelling_cpy(current(), result->data.var.name, MAX_LITERAL_NAME);
	next_tok();
	if (current_is(tok_equal))
	{
		next_tok();
		result->data.var.expr = parse_expression();
	}
	if (_parse_err)
	{
		ast_destroy_declaration(result);
		return NULL;
	}
	result->tokens.end = current();
	return result;
}

/*
<declaration> :: = <var_declaration> ";"
				| <type-specifier> ";"
				| <function_declaration> ";"

<var_declaration> ::= <type_specifier> <id> [=<exp>]
<function_declaration> ::= <type-specifier> <id> "(" [ <function_params ] ")"
<type_specifier> ::= "int" | <struct_specifier>
*/
ast_declaration_t* try_parse_declaration_opt_semi(bool* found_semi)
{
	token_t* start = current();

	ast_type_spec_t* type = try_parse_type_spec();

	if (!type)
		return NULL;

	ast_declaration_t* result = (ast_declaration_t*)malloc(sizeof(ast_declaration_t));
	memset(result, 0, sizeof(ast_declaration_t));
	result->tokens.start = current();

	if (current_is(tok_identifier))
	{
		if (next_is(tok_l_paren))
		{
			//function			
			result->kind = decl_func;
			result->data.func.return_type = type;
			expect_cur(tok_identifier);
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
			result->data.var.type = type;
			tok_spelling_cpy(current(), result->data.var.name, MAX_LITERAL_NAME);
			next_tok();
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
		result->data.type = *type;
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

/*
<primary-exp> ::= <id> | <literal> | "(" <exp> ")"
*/
ast_expression_t* try_parse_primary_expr()
{
	ast_expression_t* expr = NULL;

	if (current_is(tok_identifier))
	{
		//<factor> ::= <id>
		expr = parse_identifier();
	}
	else if (current_is(tok_num_literal))
	{
		//<factor> ::= <int>
		expr = _alloc_expr();
		expr->kind = expr_int_literal;
		expr->data.const_val = (uint32_t)(long)current()->data;
		next_tok();
	}
	else if (current_is(tok_l_paren))
	{
		//<factor ::= "(" <exp> ")"
		next_tok();
		expr = parse_expression();
		expect_cur(tok_r_paren);
		next_tok();
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
	static tok_kind postfix_ops[] = { tok_l_paren, tok_fullstop, tok_plusplus, tok_minusminus, tok_invalid };

	ast_expression_t* primary = try_parse_primary_expr();

	if (!primary)
		return NULL;

	while (tok_in_set(current()->kind, postfix_ops))
	{
		ast_expression_t* expr = _alloc_expr();
		if (current_is(tok_l_paren))
		{
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

			expr->tokens = primary->tokens;
			expr->kind = expr_binary_op;
			expr->data.binary_op.lhs = primary;
			expr->data.binary_op.rhs = parse_identifier();
			expr->data.binary_op.operation = op_member_access;
			expr->tokens.end = current();
		}
		else if (current_is(tok_plusplus) || current_is(tok_minusminus))
		{
			//postfix inc/dec
			expr->tokens = primary->tokens;
			expr->kind = expr_postfix_op;
			expr->data.unary_op.operation = _get_postfix_operator(current());
			expr->data.unary_op.expression = primary;
			next_tok();
			expr->tokens.end = current();
		}
		primary = expr;
	}
	return primary;
}

ast_expression_t* parse_cast_expr();

/*
<unary-exp> ::= <postfix-exp>
				| <unary-op> <cast-exp>
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
		result->data.expr = parse_expression();
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
			result->data.for_smnt.condition->data.const_val = 1;
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
	else
	{
		//<statement ::= <exp-option> ";"
		result->kind = smnt_expr;
		result->data.expr = parse_optional_expression(tok_semi_colon);
		expect_cur(tok_semi_colon);
		next_tok();
	}
	if (_parse_err)
	{
		ast_destroy_statement(result);
		return NULL;
	}
	result->tokens.end = current();
	return result;
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

//<translation_unit> :: = { <function> | <declaration> }
ast_trans_unit_t* parse_translation_unit(token_t* tok)
{
	_cur_tok = tok;

	ast_trans_unit_t* result = (ast_trans_unit_t*)malloc(sizeof(ast_trans_unit_t));
	memset(result, 0, sizeof(ast_trans_unit_t));
	result->tokens.start = current();
	_parse_err = false;

	while (!current_is(tok_eof))
	{
		bool found_semi;
		ast_declaration_t* decl = try_parse_declaration_opt_semi(&found_semi);

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
				{
					ast_destory_translation_unit(result);
					return NULL;
				}
				if (last_blk)
					last_blk->next = blk;
				else
					func->blocks = blk;
				last_blk = blk;
				last_blk->next = NULL;
			}
			next_tok();
		}
		else if(!found_semi)
		{
			report_err(ERR_SYNTAX, "expected ';' after declaration of %s", ast_declaration_name(decl));
		}
		if (_parse_err)
			break;
		_add_decl_to_tl(result, decl);		
	}
	result->tokens.end = current();

	expect_cur(tok_eof);
	return result;
}