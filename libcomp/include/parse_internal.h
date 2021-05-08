#pragma once

#include "token.h"
#include "ast.h"

typedef enum
{
	dpc_normal, //parsing a normal declararion list
	dpc_struct, //parsing a struct member declaration (permits bit size - 'int x : 1;', disallows initialiser
	dpc_param //parsing a function parameter declaration (no initialiser is allowed)
}decl_parse_context;

ast_decl_list_t try_parse_decl_list(decl_parse_context context);

ast_statement_t* parse_statement();
ast_expression_t* parse_expression();
ast_expression_t* try_parse_expression();
ast_block_item_t* parse_block_list();
ast_expression_t* parse_constant_expression();
void parse_type_init();
ast_expression_t* parse_alloc_expr();

//attempt to parse a type_spec, return flags indicating any qualifiers
ast_type_spec_t* try_parse_type_spec(uint32_t* flag_result);

//given a previously parsed type_spec, create a type_ref (possibly parsing pointers)
//if a named function pointer is encountered the name is returned via identifier
typedef struct { ast_type_ref_t* type; token_t* identifier; } parse_type_ref_result_t;
parse_type_ref_result_t parse_type_ref(ast_type_spec_t* spec, uint32_t flags);

ast_func_params_t* parse_function_parameters();

ast_type_ref_t* try_parse_type_ref();
ast_expression_t* try_parse_literal();
ast_expression_t* parse_optional_expression(tok_kind term_tok);
ast_expression_t* opt_parse_array_size();

void parse_on_enter_block();
void parse_on_leave_block();
void parse_register_alias_name(const char* name); //typedef name

//helper functions
bool expect_cur(tok_kind k);
void* parse_err(int err, const char* format, ...);
bool parse_seen_err();

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


