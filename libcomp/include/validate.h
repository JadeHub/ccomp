#pragma once

#include "ast.h"

typedef struct
{
	ast_trans_unit_t* ast;
	ast_declaration_t* functions;
	ast_declaration_t* variables;
}valid_trans_unit_t;

valid_trans_unit_t* validate_tl(ast_trans_unit_t*);