#include "validate.h"
#include "diag.h"
#include "var_set.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/******************************/
/*       named_ptr_set        */
/******************************/

typedef struct named_ptr_item
{
	const char* name;
	void* ptr;
	struct named_ptr_item* next;
}named_ptr_item_t;

typedef struct
{
	named_ptr_item_t* items;
}named_ptr_set_t;

named_ptr_set_t* nps_create()
{
	named_ptr_set_t* set = (named_ptr_set_t*)malloc(sizeof(named_ptr_set_t));
	memset(set, 0, sizeof(named_ptr_set_t));
	return set;
}

void nps_destroy(named_ptr_set_t* set)
{
	named_ptr_item_t* item = set->items;
	named_ptr_item_t* next;

	while (item)
	{
		next = item->next;
		free((void*)item->name);
		free(item);
		item = next;
	}
	free(set);
}

void* nps_lookup(named_ptr_set_t* set, const char* name)
{
	named_ptr_item_t* item = set->items;

	while (item)
	{
		if (strcmp(item->name, name) == 0)
			return item->ptr;
		item = item->next;
	}
	return NULL;
}

void nps_insert(named_ptr_set_t* set, const char* name, void* ptr)
{
	named_ptr_item_t* item = (named_ptr_item_t*)malloc(sizeof(named_ptr_item_t));
	memset(item, 0, sizeof(named_ptr_item_t));
	item->name = strdup(name);
	item->ptr = ptr;
	item->next = set->items;
	set->items = item;
}

/******************************/
static named_ptr_set_t* _functions;
static var_set_t* _var_set;

bool process_statement(ast_statement_t* smnt);

static bool comp_func_decl_params(ast_function_decl_t* fn1, ast_function_decl_t* fn2)
{
	return fn1->param_count == fn2->param_count;
}

bool process_variable_definition(ast_var_decl_t* decl)
{
	if (!decl)
		return true;

	stack_var_data_t* existing = var_cur_block_find(_var_set, decl->name);

	if (existing)
	{
		diag_err(decl->tokens.start, ERR_DUP_VAR, "var %s already declared at %s",
			decl->name, diag_pos_str(var_get_tok(existing)));
		return false;
	}
	var_decl_stack_var(_var_set, decl);
	return true;
}

bool process_variable_reference(ast_expression_t* expr)
{
	if (!expr)
		return true;

	stack_var_data_t* existing = var_cur_block_find(_var_set, expr->data.var_reference.name);
	if (!existing)
	{
		diag_err(expr->tokens.start, ERR_UNKNOWN_VAR, "var %s not defined",expr->data.var_reference.name);
		return false;
	}
	return true;
}

bool process_variable_assignment(ast_expression_t* expr)
{
	if (!expr)
		return true;
	stack_var_data_t* existing = var_cur_block_find(_var_set, expr->data.assignment.name);
	if (!existing)
	{
		diag_err(expr->tokens.start, ERR_UNKNOWN_VAR, "var %s not defined", expr->data.assignment.name);
		return false;
	}

	return true;
}

bool process_func_call(ast_expression_t* expr)
{
	if (!expr)
		return true;

	ast_function_decl_t* existing = (ast_function_decl_t*)nps_lookup(_functions, expr->data.func_call.name);
	if (!existing)
	{
		diag_err(expr->tokens.start, ERR_UNKNOWN_FUNC, "function %s not defined", expr->data.func_call.name);
		return false;
	}

	if (existing->param_count != expr->data.func_call.param_count)
	{
		diag_err(expr->tokens.start, ERR_INVALID_PARAMS, "incorrect params in call to function %s", expr->data.func_call.name);
		return false;
	}
	return true;
}

bool process_expression(ast_expression_t* expr)
{
	if (!expr)
		return true;

	switch (expr->kind)
	{
		case expr_postfix_op:
		case expr_unary_op:
			return process_expression(expr->data.unary_op.expression);
			break;
		case expr_binary_op:
			if (!process_expression(expr->data.binary_op.lhs))
				return false;
			return process_expression(expr->data.binary_op.rhs);
			break;
		case expr_int_literal:
			break;
		case expr_assign:
			if (!process_expression(expr->data.assignment.expr))
				return false;
			return process_variable_assignment(expr);
			break;
		case expr_var_ref:
			return process_variable_reference(expr);
			break;
		case expr_condition:
			if (!process_expression(expr->data.condition.cond))
				return false;
			if (!process_expression(expr->data.condition.true_branch))
				return false;
			return process_expression(expr->data.condition.false_branch);
			break;
		case expr_func_call:
			return process_func_call(expr);
			break;
		case expr_null:
			break;
	}

	return true;
}

bool process_block_item(ast_block_item_t* block)
{
	if (!block)
		return true;

	if (block->kind == blk_smnt)
	{
		return process_statement(block->smnt);
	}
	else if (block->kind == blk_var_def)
	{
		return process_variable_definition(block->var_decl);
	}	
	return false;
}

bool process_statement(ast_statement_t* smnt)
{
	if (!smnt)
		return true;

	switch (smnt->kind)
	{
	case smnt_expr:
		return process_expression(smnt->data.expr);
		break;
	case smnt_do:
	case smnt_while:
		if (!process_expression(smnt->data.while_smnt.condition))
			return false;
		if (!process_statement(smnt->data.while_smnt.statement))
			return false;
		break;
	case smnt_for:
	case smnt_for_decl:
		if (!process_variable_definition(smnt->data.for_smnt.init_decl))
			return false;
		if (!process_expression(smnt->data.for_smnt.init))
			return false;
		if (!process_expression(smnt->data.for_smnt.condition))
			return false;
		if (!process_expression(smnt->data.for_smnt.post))
			return false;
		if (!process_statement(smnt->data.for_smnt.statement))
			return false;
		break;
	case smnt_if:
		if (!process_expression(smnt->data.if_smnt.condition))
			return false;
		if (!process_statement(smnt->data.if_smnt.true_branch))
			return false;
		if (!process_statement(smnt->data.if_smnt.false_branch))
			return false;
		break;
	case smnt_compound:
		return ast_visit_block_items(smnt->data.compound.blocks, process_block_item);
		break;
	case smnt_return:
		return process_expression(smnt->data.expr);
	}

	return true;
}

bool process_function_decl(ast_function_decl_t* fn)
{
	ast_function_decl_t* existing = (ast_function_decl_t*)nps_lookup(_functions, fn->name);
	if (existing)
	{
		if (existing->blocks && fn->blocks)
		{
			//There can only be one definition of a function
			diag_err(fn->tokens.start, ERR_FUNC_DUP_BODY, 
				"function '%s' already has a body", fn->name);
			return false;
		}
		if (!comp_func_decl_params(existing, fn))
		{
			diag_err(fn->tokens.start, ERR_FUNC_DIFF_PARAMS,
				"duplicate declarations of function '%s' have different parameter lists",
				fn->name);
			return false;
		}
	}
	else
	{
		nps_insert(_functions, fn->name, fn);
	}

	_var_set = var_init_set(fn);
	ast_visit_block_items(fn->blocks, process_block_item);
	var_destory_set(_var_set);
	return true;
}

void validate_tl(ast_trans_unit_t* tl)
{
	_functions = nps_create();

	ast_visit_functions(tl, process_function_decl);

	nps_destroy(_functions);
}