#include "var_set.h"
#include "diag.h"

#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>

static var_data_t* _make_stack_var(int bsp_offset, const char* name)
{
	var_data_t* var = (var_data_t*)malloc(sizeof(var_data_t));
	memset(var, 0, sizeof(var_data_t));
	var->bsp_offset = bsp_offset;
	strcpy(var->name, name);
	return var;
}

var_data_t* var_cur_block_find(var_set_t* vars, const char* name)
{
	var_data_t* var = vars->vars;
	while (var && var->kind != var_block_mark)
	{
		if (strcmp(name, var->name) == 0)
			return var;

		var = var->next;
	}
	return NULL;
}

void var_destory_set(var_set_t* vars)
{
	var_data_t* var;
	var_data_t* next;

	var = vars->vars;

	while (var)
	{
		next = var->next;
		free(var);
		var = next;
	}
	free(vars);
}

var_set_t* var_init_set()
{
	var_set_t* set = (var_set_t*)malloc(sizeof(var_set_t));
	memset(set, 0, sizeof(var_set_t));
	var_enter_block(set); //make an initial block marker with bsp_offset of 0
	set->global_marker = set->vars;
	return set;
}

void var_enter_function(var_set_t* vars, ast_function_decl_t* fn)
{
	/* add the function parameters */
	if (fn->params)
	{
		//ast_function_param_t* param = fn->params;
		ast_declaration_t* param = fn->params;

		//skip 4 bytes of stack for the return value & 4 bytes for ebp which is pushed in the fn prologue		
		int offset = 8;

		if (fn->return_type->size > 4)
			offset += 4; //add 4 bytes for the return value pointer

		while (param)
		{			
			var_data_t* var = _make_stack_var(offset, param->data.var.name);
			var->kind = var_param;
			var->data.decl = &param->data.var;
			var->next = vars->vars;
			vars->vars = var;

			offset += param->data.var.type->size;
			param = param->next;
		}
	}
}

void var_leave_function(var_set_t* vars)
{
	// remove all before the global block marker
	var_data_t* var = vars->vars;
	while (var)
	{
		if (var == vars->global_marker)
		{
			vars->bsp_offset = 0;
			vars->vars = vars->global_marker;
			return;
		}

		var_data_t* next = var->next;
		free(var);
		var = next;
	}
	assert(false);
}


void var_enter_block(var_set_t* vars)
{
	var_data_t* var = _make_stack_var(vars->bsp_offset, "");
	var->kind = var_block_mark;
	// add to start of list
	var->next = vars->vars;
	vars->vars = var;
}

int var_leave_block(var_set_t* vars)
{
	int bsp_start = vars->bsp_offset;

	// remove all up to and including the most recent block marker
	var_data_t* var = vars->vars;
	while (var)
	{
		if (var->kind == var_block_mark)
		{
			int bsp_end = var->bsp_offset;
			vars->vars = var->next;
			vars->bsp_offset = bsp_end;
			free(var);
			return bsp_end - bsp_start;
		}

		var_data_t* next = var->next;
		free(var);
		var = next;
	}
	assert(false);
	return 0;
}

var_data_t* var_decl_stack_var(var_set_t* vars, ast_var_decl_t* var_decl)
{
	var_data_t* var = var_cur_block_find(vars, var_decl->name);
	if (var)
	{
		return NULL;
	}
	
	vars->bsp_offset -= var_decl->type->size;
	var = _make_stack_var(vars->bsp_offset, var_decl->name);
	var->kind = var_stack;
	var->data.decl = var_decl;
	
	// add to start of list
	var->next = vars->vars;
	vars->vars = var;
		
	return var;
}

var_data_t* var_decl_global_var(var_set_t* vars, ast_var_decl_t* decl)
{
	var_data_t* var = _make_stack_var(vars->bsp_offset, decl->name);
	var->kind = var_global;
	var->data.decl = decl;

	//insert after global marker
	var->next = vars->global_marker->next;
	vars->global_marker->next = var;

	return var;
}

var_data_t* var_find(var_set_t* vars, const char* name)
{
	var_data_t* var = vars->vars;
	// Search from most recently declared variable
	while (var)
	{
		if (strcmp(name, var->name) == 0)
			return var;
		var = var->next;
	}
		
	return NULL;
}

token_t* var_get_tok(var_data_t* var)
{
	return NULL;
	/*assert(var->kind != var_block_mark);
	
	if (var->kind == var_stack || var->kind == var_global)
		return var->data.decl->tokens.start;
	return var->data.param->tokens.start;*/
}