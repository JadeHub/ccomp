#pragma once

#include "token.h"
#include "ast.h"

ast_declaration_t* try_parse_declaration();
ast_statement_t* parse_statement();
ast_expression_t* parse_expression();
ast_expression_t* try_parse_expression();
ast_block_item_t* parse_block_item();
ast_expression_t* parse_constant_expression();
ast_type_spec_t* try_parse_pointer_decl_spec();
ast_expression_t* try_parse_literal();
ast_expression_t* parse_optional_expression(tok_kind term_tok);

//helper functions
bool expect_cur(tok_kind k);
void expect_next(tok_kind k);
void* report_err(int err, const char* format, ...);

extern token_t* _cur_tok;

static inline token_t* current()
{
	return _cur_tok;
}

static inline bool current_is(tok_kind k)
{
	return _cur_tok->kind == k;
}

static inline token_t* next_tok()
{
	_cur_tok = _cur_tok->next;
	return _cur_tok;
}

static inline bool next_is(tok_kind k)
{
	return _cur_tok->next->kind == k;
}


