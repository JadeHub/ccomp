#pragma once

#include "ast.h"
#include "diag.h"

typedef struct stack_var_data
{
	ast_var_decl_t* decl;
	int bsp_offset;

	struct stack_var_data* next;
}stack_var_data_t;

/*
Data set representing the current set of known variables
*/
typedef struct
{
	int bsp_offset;
	stack_var_data_t* vars;

	ast_function_decl_t* current_fn;
}var_set_t;

var_set_t* var_init_set(ast_function_decl_t*);
void var_destory_set(var_set_t*);

//ast_function_decl_t* var_enter_function(var_set_t*, ast_function_decl_t*);
//void var_leave_function(var_set_t*, ast_function_decl_t*);

void var_enter_block(var_set_t*);

/* returns new stack pointer*/
int var_leave_block(var_set_t*);

//enter block/leave block

stack_var_data_t* var_decl_stack_var(var_set_t*, ast_var_decl_t*);

stack_var_data_t* var_stack_find(var_set_t*, const char* name);

/*
Returns negative offset for stack variables, positive stack offset for function params and 0 for failure
*/
int var_get_bsp_offset(var_set_t*, const char* name);
