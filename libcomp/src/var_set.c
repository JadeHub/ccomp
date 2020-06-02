#include "var_set.h"
#include "diag.h"

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>

static stack_var_data_t* _make_stack_var(int bsp_offset)
{
	stack_var_data_t* var = (stack_var_data_t*)malloc(sizeof(stack_var_data_t));
	memset(var, 0, sizeof(stack_var_data_t));
	var->bsp_offset = bsp_offset;
	return var;
}

static stack_var_data_t* _find_in_current_block(var_set_t* vars, const char* name)
{
	stack_var_data_t* var = vars->vars;
	while (var && var->decl != NULL)
	{
		if (strcmp(name, var->decl->decl_name) == 0)
			return var;

		var = var->next;
	}
	return NULL;
}

void var_destory_set(var_set_t* vars)
{
	stack_var_data_t* var;
	stack_var_data_t* next;

	var = vars->vars;

	while (var)
	{
		next = var->next;
		free(var);
		var = next;
	}
	free(vars);
}

var_set_t* var_init_set(ast_function_decl_t* fn)
{
	var_set_t* vars = (var_set_t*)malloc(sizeof(var_set_t));
	memset(vars, 0, sizeof(var_set_t));
	vars->current_fn = fn;
	var_enter_block(vars); //make an initial block marker with bsp_offset of 0
	return vars;
}

void var_enter_block(var_set_t* vars)
{
	stack_var_data_t* var = _make_stack_var(vars->bsp_offset);

	/* add to start of list */
	var->next = vars->vars;
	vars->vars = var;
}

int var_leave_block(var_set_t* vars)
{
	int bsp_start = vars->bsp_offset;

	//Walk backwards looking for the block marker
	stack_var_data_t* var = vars->vars;
	while (var)
	{
		if (var->decl == NULL)
		{
			int bsp_end = var->bsp_offset;
			vars->vars = var->next;
			vars->bsp_offset = bsp_end;
			free(var);
			return bsp_end- bsp_start /*- bsp_end*/;
		}

		stack_var_data_t* next = var->next;
		free(var);
		var = next;
	}
	assert(false);
	return 0;
}

stack_var_data_t* var_decl_stack_var(var_set_t* vars, ast_var_decl_t* var_decl)
{
	stack_var_data_t* var = _find_in_current_block(vars, var_decl->decl_name);
	if (var)
	{
		diag_err(var_decl->tokens.start, ERR_SYNTAX, "Duplicate var decl %s", var_decl->decl_name);
		return NULL;
	}
	
	vars->bsp_offset -= 4;
	var = _make_stack_var(vars->bsp_offset);
	var->decl = var_decl;
	
	/* add to start of list */
	var->next = vars->vars;
	vars->vars = var;
		
	return var;
}

int var_get_bsp_offset(var_set_t* vars, const char* name)
{
	stack_var_data_t* stack_var = var_stack_find(vars, name);

	if (stack_var)
		return stack_var->bsp_offset;

	/* Search the function parameters */
	if (vars->current_fn && vars->current_fn->params)
	{
		ast_function_param_t* param = vars->current_fn->params;
		//params are stored in reverse order so calculate the offset of the
		//last param and work down from there
		int offset = 4 + (4 * vars->current_fn->param_count);
		while (param)
		{
			if (strcmp(name, param->name) == 0)
				return offset;
			offset -= 4;
			param = param->next;
		}
	}
	return 0;
}

stack_var_data_t* var_stack_find(var_set_t* vars, const char* name)
{
	stack_var_data_t* var = vars->vars;
	/* Search from most recently declared variable*/
	while (var)
	{
		if (var->decl)
		{
			if (strcmp(name, var->decl->decl_name) == 0)
				return var;
		}
		var = var->next;
	}
		
	return NULL;
}
