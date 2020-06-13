#include "var_set.h"
#include "diag.h"

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>

static stack_var_data_t* _make_stack_var(int bsp_offset, const char* name)
{
	stack_var_data_t* var = (stack_var_data_t*)malloc(sizeof(stack_var_data_t));
	memset(var, 0, sizeof(stack_var_data_t));
	var->bsp_offset = bsp_offset;
	strcpy(var->name, name);
	return var;
}

stack_var_data_t* var_cur_block_find(var_set_t* vars, const char* name)
{
	stack_var_data_t* var = vars->vars;
	while (var && var->kind != stack_var_block_mark)
	{
		if (strcmp(name, var->name) == 0)
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

	/* add the function parameters to the function's block */
	if (fn->params)
	{
		ast_function_param_t* param = fn->params;
		//params are stored in reverse order so calculate the offset of the
		//last param and work down from there
		int offset = 4 + (4 * fn->param_count);
		while (param)
		{
			stack_var_data_t* var = _make_stack_var(offset, param->name);
			var->kind = stack_var_param;
			var->data.param = param;
			var->next = vars->vars;
			vars->vars = var;
			offset -= 4;
			param = param->next;
		}
	}

	return vars;
}

void var_enter_block(var_set_t* vars)
{
	stack_var_data_t* var = _make_stack_var(vars->bsp_offset, "");
	var->kind = stack_var_block_mark;
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
		if (var->kind == stack_var_block_mark)
		{
			int bsp_end = var->bsp_offset;
			vars->vars = var->next;
			vars->bsp_offset = bsp_end;
			free(var);
			return bsp_end - bsp_start;
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
	stack_var_data_t* var = var_cur_block_find(vars, var_decl->name);
	if (var)
	{
		return NULL;
	}
	
	vars->bsp_offset -= 4;
	var = _make_stack_var(vars->bsp_offset, var_decl->name);
	var->kind = stack_var_decl;
	var->data.decl = var_decl;
	
	/* add to start of list */
	var->next = vars->vars;
	vars->vars = var;
		
	return var;
}

int var_get_bsp_offset(var_set_t* vars, const char* name)
{
	stack_var_data_t* stack_var = var_stack_find(vars, name);
	return stack_var ? stack_var->bsp_offset : 0;
}

stack_var_data_t* var_stack_find(var_set_t* vars, const char* name)
{
	stack_var_data_t* var = vars->vars;
	/* Search from most recently declared variable*/
	while (var)
	{
		if (strcmp(name, var->name) == 0)
			return var;
		var = var->next;
	}
		
	return NULL;
}

token_t* var_get_tok(stack_var_data_t* var)
{
	assert(var->kind != stack_var_block_mark);
	return var->kind == stack_var_decl ?
		var->data.decl->tokens.start :
		var->data.param->tokens.start;
}