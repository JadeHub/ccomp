#include "token.h"
#include "parse.h"
#include "diag.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

ast_expression_t* parse_expression();

static diag_cb _diag_cb;
static token_t* _start_tok = NULL; 
static token_t* _cur_tok = NULL;

static void _err_diag(uint32_t err, const char* format, ...)
{
	char buff[512];

	va_list args;
	va_start(args, format);
	vsnprintf(buff, 512, format, args);
	va_end(args);

	_diag_cb(_cur_tok, err, buff);
}

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

static void expect_cur(tok_kind k)
{
	if (!current_is(k))
		_err_diag(ERR_SYNTAX, "Expected %s", tok_kind_name(k));
}

static void expect_next(tok_kind k)
{
	next_tok();
	expect_cur(k);
}

static bool _is_unary_op(token_t* tok)
{
	switch (tok->kind)
	{
	case tok_minus:
	case tok_tilda:
	case tok_exclaim:
		return true;
	}
	return false;
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
	}

	_err_diag(ERR_SYNTAX, "Unknown unary op {}", tok_kind_name(tok->kind));
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
	_err_diag(ERR_SYNTAX, "Unknown binary op {}", tok_kind_name(tok->kind));
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
<program> ::= <function>
<function> ::= "int" <id> "(" ")" "{" <statement> "}"
<statement> ::= "return" <exp> ";"
			  | <exp> ";"
			  | "int" <id> [ = <exp>] ";"
<exp> ::= <assignment_exp>
<assignment_exp> ::= <id> "=" <exp> | <logical-or-exp>
<logical-or-exp> ::= <logical-and-exp> { "||" <logical-and-exp> }
<logical-and-exp> ::= <bitwise-or> { "&&" <bitwise-or> }

<bitwise-or> :: = <bitwise-xor> { ("|") <bitwise-xor> }
<bitwise-xor> :: = <bitwise-and> { ("^") <bitwise-and> }
<bitwise-and> :: = <equality-exp> { ("&") <equality-exp> }

<equality-exp> ::= <relational-exp> { ("!=" | "==") <relational-exp> }
<relational-exp> ::= <bitshift-exp> { ("<" | ">" | "<=" | ">=") <bitshift-exp> }
<bitshift-exp> ::= <additive-exp> { ("<<" | ">>") <additive-exp> }
<additive-exp> ::= <term> { ("+" | "-") <term> }
<term> ::= <factor> { ("*" | "/") <factor> }
<factor> ::= "(" <exp> ")" | <unary_op> <factor> | <int> | <id>
<unary_op> ::= "!" | "~" | "-"
*/

tok_kind assignment_ops[] = { tok_equal, tok_invalid };
tok_kind logical_or_ops[] = { tok_pipepipe, tok_invalid };
tok_kind logical_and_ops[] = { tok_ampamp, tok_invalid };

tok_kind bitwise_or_ops[] = { tok_pipe, tok_invalid };
tok_kind bitwise_xor_ops[] = { tok_caret, tok_invalid };
tok_kind bitwise_and_ops[] = { tok_amp, tok_invalid };
tok_kind equality_ops[] = { tok_equalequal, tok_exclaimequal, tok_invalid };
tok_kind relational_ops[] = { tok_lesser, tok_lesserequal, tok_greater, tok_greaterequal, tok_invalid };
tok_kind bitshift_ops[] = { tok_lesserlesser, tok_greatergreater, tok_invalid };
tok_kind additive_ops[] = { tok_plus, tok_minus, tok_invalid };

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

		//our two factors are now expr and rhs_expr
		//build a new binary op expression for expr
		ast_expression_t* cur_expr = expr;
		expr = _alloc_expr();
		expr->tokens.start = start;
		expr->kind = expr_binary_op;
		expr->data.binary_op.operation = op;
		expr->data.binary_op.lhs = cur_expr;
		expr->data.binary_op.rhs = rhs_expr;
	}
	expr->tokens.end = current();
	return expr;
}

/*
<factor> ::= "(" <exp> ")" | <unary_op> <factor> | <int>
*/
ast_expression_t* parse_factor()
{
	ast_expression_t* expr = NULL;

	if (current_is(tok_l_paren))
	{
		//<factor ::= "(" <exp> ")"
		next_tok();
		expr = parse_expression();
		expect_cur(tok_r_paren);
		next_tok();
	}
	else if (_is_unary_op(current()))
	{
		//<factor> ::= <unary_op> <factor>
		expr = _alloc_expr();
		expr->kind = expr_unary_op;
		expr->data.unary_op.operation = _get_unary_operator(current());
		next_tok();
		expr->data.unary_op.expression = parse_factor();
	}
	else if (current_is(tok_num_literal))
	{
		//<factor> ::= <int>
		expr = _alloc_expr();
		expr->kind = expr_int_literal;
		expr->data.const_val = (uint32_t)current()->data;
		next_tok();
	}
	else if (current_is(tok_identifier))
	{
		//<factor> ::= <id>
		expr = _alloc_expr();
		expr->kind = expr_var_ref;
		tok_spelling_cpy(current(), expr->data.var_reference.name, MAX_LITERAL_NAME);
		next_tok();
	}
	else
	{
		_err_diag(ERR_SYNTAX, "parse_factor failed");
	}
	if(expr)
		expr->tokens.end = current();
	return expr;
}

/*
<term> ::= <factor> { ("*" | "/" | "%") <factor> }
*/
ast_expression_t* parse_term()
{
	ast_expression_t* expr = parse_factor();
	while (current()->kind == tok_star || current()->kind == tok_slash || current()->kind == tok_percent)
	{
		op_kind op = _get_binary_operator(current());
		next_tok();
		ast_expression_t* rhs_expr = parse_factor();

		//our two factors are now expr and rhs_expr
		//build a new binary op expression for expr
		ast_expression_t* cur_expr = expr;
		expr = _alloc_expr();		
		expr->kind = expr_binary_op;
		expr->data.binary_op.operation = op;
		expr->data.binary_op.lhs = cur_expr;
		expr->data.binary_op.rhs = rhs_expr;
	}
	expr->tokens.end = current();
	return expr;
}

/*
<additive-exp> ::= <term> { ("+" | "-") <term> }
*/
ast_expression_t* parse_additive_expr()
{
	return parse_binary_expression(additive_ops, parse_term);
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
<assignment-exp> ::= <id> "=" <exp> | <logical-or-exp>
*/
ast_expression_t* parse_assignment_expression()
{
	ast_expression_t* expr;

	if (current_is(tok_identifier) && _cur_tok->next->kind == tok_equal)
	{
		expr = _alloc_expr();
		expr->kind = expr_assign;
		tok_spelling_cpy(current(), expr->data.assignment.name, MAX_LITERAL_NAME);
		expect_next(tok_equal);
		next_tok();
		expr->data.assignment.expr = parse_expression();
	}
	else
	{
		expr = parse_logical_or_expr();
	}
	expr->tokens.end = current();
	return expr;
}

ast_expression_t* parse_expression()
{
	return parse_assignment_expression();
}

/*
<statement> :: = "return" < exp > ";"
| < exp> ";"
| "int" < id > [= <exp>] ";"
*/
ast_statement_t* parse_statement()
{
	ast_statement_t* result = (ast_statement_t*)malloc(sizeof(ast_statement_t));
	memset(result, 0, sizeof(ast_statement_t));
	result->tokens.start = current();
	if (current_is(tok_return))
	{
		//<statement> :: = "return" < exp > ";"
		next_tok();
		result->kind = smnt_return;
		result->expr = parse_expression();
	}
	else if (current_is(tok_int))
	{
		//<statement ::= "int" < id > [= <exp>] ";"
		expect_next(tok_identifier);
		result->kind = smnt_var_decl;
		tok_spelling_cpy(current(), result->decl_name, 32);
		next_tok();
		if (current_is(tok_equal))
		{
			next_tok();
			result->expr = parse_expression();
		}
	}
	else
	{
		//<statement ::= <exp> ";"
		result->kind = smnt_expr;
		result->expr = parse_expression();
	}
	expect_cur(tok_semi_colon);
	next_tok();
	result->tokens.end = current();
	return result;
}

/*
<function> ::= "int" <id> "(" ")" "{" { <statement> } "}"
*/
ast_function_decl_t* parse_function_decl()
{
	ast_function_decl_t* result = (ast_function_decl_t*)malloc(sizeof(ast_function_decl_t));
	memset(result, 0, sizeof(ast_function_decl_t));
	result->tokens.start = current();

	/*return type*/
	expect_cur(tok_int);

	/*function name*/
	expect_next(tok_identifier);
	tok_spelling_cpy(current(), result->name, 32);

	/*(*/
	expect_next(tok_l_paren);

	/*)*/
	expect_next(tok_r_paren);

	/*{*/
	expect_next(tok_l_brace);

	next_tok();

	ast_statement_t* last_smnt = NULL;

	while (!current_is(tok_r_brace))
	{
		ast_statement_t* smnt = parse_statement();
		if (last_smnt)
			last_smnt->next = smnt;
		else
			result->statements = smnt;
		last_smnt = smnt;
		last_smnt->next = NULL;
	}
	next_tok();
	result->tokens.end = current();
	return result;
}

ast_trans_unit_t* parse_translation_unit(token_t* tok, diag_cb dcb)
{
	_start_tok = _cur_tok = tok;
	_diag_cb = dcb;

	ast_trans_unit_t* result = (ast_trans_unit_t*)malloc(sizeof(ast_trans_unit_t));

	result->function = parse_function_decl();

	return result;
}