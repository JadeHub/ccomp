#pragma once

#include "token.h"
#include "ast.h"

void parse_init(token_t* tok);
ast_trans_unit_t* parse_translation_unit();

ast_statement_t* parse_statement();
ast_expression_t* parse_expression();
ast_block_item_t* parse_block_item();
ast_expression_t* parse_unary_expr();
ast_expression_t* parse_constant_expression();
ast_type_spec_t* try_parse_pointer_decl_spec();
ast_expression_t* try_parse_literal();