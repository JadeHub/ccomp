#pragma once

#include "ast.h"

#include <libj/include/hash_table.h>

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

	hash_table_t* string_literals;
	
}valid_trans_unit_t;

valid_trans_unit_t* sem_analyse(ast_trans_unit_t*);
void tl_destroy(valid_trans_unit_t*);