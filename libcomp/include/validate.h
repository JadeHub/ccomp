#pragma once

#include "ast.h"

typedef struct
{
	ast_trans_unit_t* ast;

	//global function, var and type definitions
	ast_declaration_t* functions;
	ast_declaration_t* variables;
	ast_declaration_t* types;
}valid_trans_unit_t;

valid_trans_unit_t* tl_validate(ast_trans_unit_t*);
void tl_destroy(valid_trans_unit_t*);