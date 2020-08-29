#pragma once

#include "ast.h"

typedef struct tl_decl
{
	ast_declaration_t* decl;
	struct tl_decl* next;
}tl_decl_t;

typedef struct
{
	ast_trans_unit_t* ast;

	//global function, var and type definitions

	tl_decl_t* fn_decls;
	tl_decl_t* var_decls;
	
	//used by ast_view
	tl_decl_t* type_decls;

	/*ast_declaration_t* functions;
	ast_declaration_t* variables;
	ast_declaration_t* types;*/
}valid_trans_unit_t;

valid_trans_unit_t* sem_analyse(ast_trans_unit_t*);
void tl_destroy(valid_trans_unit_t*);