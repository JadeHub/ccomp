#pragma once

#include "ast.h"
#include "decl_map.h"

typedef struct
{
	ast_trans_unit_t* ast;
	ast_declaration_t* functions;
	ast_declaration_t* variables;
	identfier_map_t* identifiers;
}valid_trans_unit_t;

valid_trans_unit_t* tl_validate(ast_trans_unit_t*);
void tl_destroy(valid_trans_unit_t*);