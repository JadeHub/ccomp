#pragma once

#include "ast.h"
#include "diag.h"

typedef struct var_data
{
	int bsp_offset;
	char name[MAX_LITERAL_NAME];

	enum
	{
		var_param,
		var_stack,
		var_global,
		var_block_mark
	}kind;

	union
	{
		ast_var_decl_t* decl;
		ast_function_param_t* param;
	}data;

	struct var_data* next;
}var_data_t;

/*
Data set representing the current set of known variables
*/
typedef struct
{
	int bsp_offset;

	var_data_t* vars;

	ast_function_decl_t* current_fn;
}var_set_t;

var_set_t* var_init_set(ast_function_decl_t*);
void var_destory_set(var_set_t*);

void var_enter_block(var_set_t*);

/* returns number of bytes to be added to esp*/
int var_leave_block(var_set_t*);

/* declare a new stack variable*/
var_data_t* var_decl_stack_var(var_set_t*, ast_var_decl_t*);
var_data_t* var_decl_global_var(var_set_t*, ast_var_decl_t*);

/* search from the current block outwards looking for a stack variable */
var_data_t* var_stack_find(var_set_t*, const char* name);
var_data_t* var_cur_block_find(var_set_t*, const char* name);

/*
Returns negative offset for stack variables, positive stack offset for function params and 0 for failure
*/
int var_get_bsp_offset(var_set_t*, const char* name);

token_t* var_get_tok(var_data_t*);
