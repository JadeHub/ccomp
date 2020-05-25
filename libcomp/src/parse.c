#include "token.h"
#include "parse.h"
#include "diag.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

static diag_cb _diag_cb;
static token_t* _start_tok = NULL; 
static token_t* _cur_tok = NULL;

static ast_expression_t* _alloc_expr()
{
	ast_expression_t* result = (ast_expression_t*)malloc(sizeof(ast_expression_t));
	memset(result, 0, sizeof(ast_expression_t));
	return result;
}

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
	}
	_err_diag(ERR_SYNTAX, "Unknown binary op {}", tok_kind_name(tok->kind));
}

ast_expression_t* parse_expression();


/*
<exp> ::= <term> { ("+" | "-") <term> }
<term> ::= <factor> { ("*" | "/") <factor> }
<factor> ::= "(" <exp> ")" | <unary_op> <factor> | <int>
*/


/*
<factor> ::= "(" <exp> ")" | <unary_op> <factor> | <int>
*/
ast_expression_t* parse_factor()
{
	ast_expression_t* expr = _alloc_expr();
	if (current_is(tok_l_paren))
	{
		//<factor ::= "(" <exp> ")"
		next_tok();
		free(expr);
		expr = parse_expression();
		expect_cur(tok_r_paren);
		next_tok();
	}
	else if (_is_unary_op(current()))
	{
		//<factor> ::= <unary_op> <factor>
		expr->kind = expr_unary_op;
		expr->data.unary_op.operation = _get_unary_operator(current());
		next_tok();
		expr->data.unary_op.expression = parse_factor();
	}
	else if (current_is(tok_num_literal))
	{
		//<factor> ::= <int>
		expr->kind = expr_int_literal;
		expr->data.const_val = (uint32_t)current()->data;
		next_tok();
	}
	else
	{
		_err_diag(ERR_SYNTAX, "parse_factor failed");
	}
	return expr;
}

/*
<term> ::= <factor> { ("*" | "/") <factor> }
*/
ast_expression_t* parse_term()
{
	ast_expression_t* expr = parse_factor();
	while (current()->kind == tok_star || current()->kind == tok_slash)
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
	return expr;
}

/*
<exp> ::= <term> { ("+" | "-") <term> }
*/
ast_expression_t* parse_expression()
{
	ast_expression_t* expr = parse_term();
	while (current()->kind == tok_plus || current()->kind == tok_minus)
	{
		op_kind op = _get_binary_operator(current());
		next_tok();
		ast_expression_t* rhs_expr = parse_term();

		//our two terms are now expr and rhs_expr
		//build a new binary op expression for expr
		ast_expression_t* cur_expr = expr;
		expr = _alloc_expr();
		expr->kind = expr_binary_op;
		expr->data.binary_op.operation = op;
		expr->data.binary_op.lhs = cur_expr;
		expr->data.binary_op.rhs = rhs_expr;

	}
	return expr;
}

ast_statement_t* parse_statement()
{
	ast_statement_t* result = (ast_statement_t*)malloc(sizeof(ast_statement_t));

	/*return*/
	expect_cur(tok_return);

	/*nnnn*/
	next_tok();
	result->expr = parse_expression();

	/*;*/
	expect_cur(tok_semi_colon);
	next_tok();

	return result;
}

ast_function_decl_t* parse_function_decl()
{
	//int fn() {return 2;}
	ast_function_decl_t* result = (ast_function_decl_t*)malloc(sizeof(ast_function_decl_t));

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

	expect_next(tok_return);
	result->return_statement = parse_statement();

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