#include "var_set.h"
#include "diag.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <stdbool.h>
#include <assert.h>

static var_data_t* _make_stack_var(int bsp_offset, const char* name)
{
	var_data_t* var = (var_data_t*)malloc(sizeof(var_data_t));
	memset(var, 0, sizeof(var_data_t));
	var->kind = var_stack;
	var->bsp_offset = bsp_offset;
	strcpy(var->name, name);
	return var;
}

static var_data_t* var_cur_block_find(var_set_t* vars, const char* name)
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

void var_enter_function(var_set_t* vars, ast_declaration_t* fn)
{
	/* add the function parameters */
	ast_func_param_decl_t* param = ast_func_decl_params(fn)->first_param;
	if (param)
	{
		//skip 4 bytes of stack for the return value & 4 bytes for ebp which is pushed in the fn prologue		
		size_t offset = 8;

		if (ast_func_decl_return_type(fn)->size > 4)
			offset += 4; //add 4 bytes for the return value pointer

		while (param)
		{
			var_data_t* var = _make_stack_var((int)offset, param->decl->name);
			var->kind = var_param;
			var->var_decl = param->decl;
			var->next = vars->vars;
			vars->vars = var;

			size_t param_sz = param->decl->type_ref->spec->size;
			offset += param_sz < 4 ? 4 : param_sz;
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

void var_leave_block(var_set_t* vars)
{
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
			return;
		}

		var_data_t* next = var->next;
		free(var);
		var = next;
	}
	assert(false);
}


var_data_t* var_decl_stack_var(var_set_t* vars, ast_declaration_t* decl)
{
	var_data_t* var = var_cur_block_find(vars, decl->name);
	if (var)
	{
		assert(false);
		return NULL;
	}
	
	size_t sz = decl->type_ref->spec->size;

	var = _make_stack_var(vars->bsp_offset - (int)sz, decl->name);
	var->kind = var_stack;
	var->var_decl = decl;

	vars->bsp_offset -= (sz + 0x01) & ~0x01;

	// add to start of list
	var->next = vars->vars;
	vars->vars = var;
		
	return var;
}

var_data_t* var_decl_global_var(var_set_t* vars, ast_declaration_t* decl)
{
	var_data_t* var = _make_stack_var(vars->bsp_offset, decl->name);
	var->kind = var_global;
	var->var_decl = decl;

	sprintf(var->global_name, "_var_%s", var->name);

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
	var;
	return NULL;
	/*assert(var->kind != var_block_mark);
	
	if (var->kind == var_stack || var->kind == var_global)
		return var->data.decl->tokens.start;
	return var->data.param->tokens.start;*/
}