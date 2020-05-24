#include "token.h"
#include "parse.h"
#include "diag.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

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

static inline bool cur_tok_is(tok_kind k)
{
	return _cur_tok->kind == k;
}

static void expect_cur(tok_kind k)
{
	if (_cur_tok->kind != k)
		_err_diag(ERR_SYNTAX, "Expected %s", tok_kind_name(k));
}

static void expect_next(tok_kind k)
{
	if (next_tok()->kind != k)
		_err_diag(ERR_SYNTAX, "Expected %s", tok_kind_name(k));
}

ast_statement_t* parse_return()
{
	ast_statement_t* result = (ast_statement_t*)malloc(sizeof(ast_statement_t));

	/*return*/
	expect_cur(tok_return);

	/*nnnn*/
	expect_next(tok_num_literal);
	result->val.val = (uint32_t)current()->data;

	/*;*/
	expect_next(tok_semi_colon);

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
	result->return_statement = parse_return();

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