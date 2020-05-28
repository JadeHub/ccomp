#include "var_set.h"

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <assert.h>

static diag_cb _diag_cb;



var_set_t* var_init_set(diag_cb cb)
{
	_diag_cb = cb;
	var_set_t* vars = (var_set_t*)malloc(sizeof(var_set_t));
	vars->bsp_offset = 0;
	vars->vars = NULL;
	return vars;
}

void var_enter_function(var_set_t* vars, ast_function_decl_t* fn)
{

}
void var_leave_function(var_set_t* vars)
{

}

stack_var_data_t* var_decl_stack_var(var_set_t* vars, ast_statement_t* smnt)
{
	assert(smnt->kind == smnt_var_decl);

	stack_var_data_t* var = var_find(vars, smnt->decl_name);

	if (var)
	{
		exit(1);
		return NULL;
	}
	
	var = (stack_var_data_t*)malloc(sizeof(stack_var_data_t));
	var->decl_smnt = smnt;
	vars->bsp_offset -= 4;
	var->bsp_offset = vars->bsp_offset;
	var->next = vars->vars;
	vars->vars = var;

	return var;
}

stack_var_data_t* var_find(var_set_t* vars, const char* name)
{
	stack_var_data_t* var = vars->vars;

	while (var)
	{
		if (strcmp(name, var->decl_smnt->decl_name) == 0)
			return var;
		var = var->next;
	}
	return NULL;
}
