#include "validate.h"
#include "diag.h"
#include "var_set.h"
#include "nps.h"

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

static struct named_ptr_set* _functions;
static struct named_ptr_set* _globals;
static var_set_t* _var_set;

bool process_statement(ast_statement_t* smnt);
bool process_function_decl(ast_declaration_t* fn);

static bool comp_func_decl_params(ast_function_decl_t* fn1, ast_function_decl_t* fn2)
{
	return fn1->param_count == fn2->param_count;
}

bool process_variable_declaration(ast_declaration_t* decl)
{
	ast_var_decl_t* var = &decl->data.var;
	if (!var)
		return true;

	var_data_t* existing = var_cur_block_find(_var_set, var->name);

	if (existing)
	{
		diag_err(decl->tokens.start, ERR_DUP_VAR, "var %s already declared at",
			var->name);
		return false;
	}
	var_decl_stack_var(_var_set, var);
	return true;
}

bool process_declaration(ast_declaration_t* var)
{
	switch (var->kind)
	{
	case decl_var:
		return process_variable_declaration(var);
	case decl_func:
		return process_function_decl(var);
	}
	return true;
}

bool process_variable_reference(ast_expression_t* expr)
{
	if (!expr)
		return true;

	var_data_t* existing = var_find(_var_set, expr->data.var_reference.name);
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
	var_data_t* existing = var_find(_var_set, expr->data.assignment.name);
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
	else if (block->kind == blk_decl)
	{
		process_declaration(block->decl);
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
		if (!process_declaration(smnt->data.for_smnt.init_decl))
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

bool process_function_decl(ast_declaration_t* decl)
{
	ast_function_decl_t* fn = &decl->data.func;
	ast_var_decl_t* var = (ast_var_decl_t*)nps_lookup(_globals, fn->name);
	if (var)
	{
		diag_err(decl->tokens.start, ERR_DUP_SYMBOL,
			"function declaration of '%s' shadows global var at",
			fn->name);
		return false;
	}

	ast_function_decl_t* existing = (ast_function_decl_t*)nps_lookup(_functions, fn->name);
	if (existing)
	{
		if (existing->blocks && fn->blocks)
		{
			//There can only be one definition of a function
			diag_err(decl->tokens.start, ERR_FUNC_DUP_BODY,
				"function '%s' already has a body", fn->name);
			return false;
		}
		if (!comp_func_decl_params(existing, fn))
		{
			diag_err(decl->tokens.start, ERR_FUNC_DIFF_PARAMS,
				"duplicate declarations of function '%s' have different parameter lists",
				fn->name);
			return false;
		}
	}
	else
	{
		nps_insert(_functions, fn->name, fn);
	}

	if (fn->blocks)
	{
		var_enter_function(_var_set, &decl->data.func);
		ast_visit_block_items(fn->blocks, process_block_item);
		var_leave_function(_var_set);
	}
	return true;
}

bool process_global_var(ast_declaration_t* decl)
{
	ast_var_decl_t* var = &decl->data.var;
	ast_function_decl_t* fn = (ast_function_decl_t*)nps_lookup(_functions, var->name);
	if (fn)
	{
		diag_err(decl->tokens.start, ERR_DUP_SYMBOL,
			"global var declaration of '%s' shadows function at",
			var->name);
		return false;
	}

	ast_var_decl_t* existing = (ast_var_decl_t*)nps_lookup(_globals, var->name);
	if (existing)
	{
		if (existing->expr && var->expr)
		{
			diag_err(decl->tokens.start, ERR_DUP_VAR,
				"multiple definition of global var '%s'. prev at",
				var->name);
			return false;
		}
		nps_remove(_globals, var->name);
	}

	if (var->expr && var->expr->kind != expr_int_literal)
	{
		diag_err(decl->tokens.start, ERR_INVALID_INIT,
			"init of global var '%s' with non const value",
			var->name);
		return false;
	}

	var_decl_global_var(_var_set, var);

	nps_insert(_globals, var->name, var);

	return true;
}

void validate_tl(ast_trans_unit_t* tl)
{
	_functions = nps_create();
	_globals = nps_create();

	_var_set = var_init_set();

	ast_declaration_t* decl = tl->decls;
	while (decl)
	{
		if(decl->kind == decl_var)
			process_global_var(decl);
		else if (decl->kind == decl_func)
			process_function_decl(decl);
		decl = decl->next;
	}

	var_destory_set(_var_set);

	nps_destroy(_functions);
	nps_destroy(_globals);
}
