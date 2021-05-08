#pragma once

#include "ast.h"
#include "diag.h"

typedef struct var_data
{
	int bsp_offset;
	char name[MAX_LITERAL_NAME];
	char global_name[MAX_LITERAL_NAME * 2];

	enum
	{
		var_param,
		var_stack,
		var_global,
		var_block_mark
	}kind;

	ast_declaration_t* var_decl;

	struct var_data* next;
}var_data_t;

/*
Data set representing the current set of known variables

list format
[block vars][block vars][block vars][fn params][global vars]

*/
typedef struct
{
	int bsp_offset;
	var_data_t* vars;
	var_data_t* global_marker;
}var_set_t;

var_set_t* var_init_set();
void var_destory_set(var_set_t*);

void var_enter_function(var_set_t* vars, ast_declaration_t*);
void var_leave_function(var_set_t* vars);

/* enter a scope block */
void var_enter_block(var_set_t*);
/* leave a block destroying local vars */
void var_leave_block(var_set_t*);

/* declare a new stack variable*/
var_data_t* var_decl_stack_var(var_set_t*, ast_declaration_t*);
var_data_t* var_decl_global_var(var_set_t*, ast_declaration_t*);

/* search from the current block outwards looking for a variable */
var_data_t* var_find(var_set_t*, const char* name);

token_t* var_get_tok(var_data_t*);



