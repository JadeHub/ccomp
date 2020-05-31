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
}var_set_t;


var_set_t* var_init_set();

void var_enter_function(var_set_t*, ast_function_decl_t*);
void var_leave_function(var_set_t*);

void var_enter_block(var_set_t*);

/* returns new stack pointer*/
int var_leave_block(var_set_t*);

//enter block/leave block

stack_var_data_t* var_decl_stack_var(var_set_t*, ast_var_decl_t*);
stack_var_data_t* var_find(var_set_t*, const char* name);
