#include "var_set.h"
#include "diag.h"

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>

static diag_cb _diag_cb;

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

var_set_t* var_init_set(diag_cb cb)
{
	_diag_cb = cb;
	var_set_t* vars = (var_set_t*)malloc(sizeof(var_set_t));
	memset(vars, 0, sizeof(var_set_t));
	var_enter_block(vars);
	return vars;
}

void var_enter_function(var_set_t* vars, ast_function_decl_t* fn)
{
}

void var_leave_function(var_set_t* vars)
{

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

stack_var_data_t* var_find(var_set_t* vars, const char* name)
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
