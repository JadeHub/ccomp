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

typedef void (*sema_user_type_def_cb)(ast_type_spec_t* spec);

typedef struct
{
	sema_user_type_def_cb user_type_def_cb;
}sema_observer_t;

void sema_init(sema_observer_t);
valid_trans_unit_t* sema_analyse(ast_trans_unit_t*);
void tl_destroy(valid_trans_unit_t*);