#include "validate.h"
#include "diag.h"
#include "var_set.h"
#include "decl_map.h"
#include "nps.h"

#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/*built in types */
/* {{token_range_t}, kind, size, struct_spec} */
static ast_type_spec_t _void_type = { {NULL, NULL}, type_void, 0, NULL };
static ast_type_spec_t _int_type = { {NULL, NULL}, type_int, 4, NULL };


static identfier_map_t* _id_map;

typedef enum
{
	proc_decl_new_def,
	proc_decl_ignore,
	proc_decl_error
}proc_decl_result;

proc_decl_result process_function_decl(ast_declaration_t* decl);
bool process_expression(ast_expression_t* expr);
bool process_statement(ast_statement_t* smnt);

static void _report_err(token_t* tok, int err, const char* format, ...)
{
	char buff[512];

	va_list args;
	va_start(args, format);
	vsnprintf(buff, 512, format, args);
	va_end(args);

	diag_err(tok, err, buff);
}

static ast_type_spec_t* _resolve_type(ast_type_spec_t* typeref)
{
	switch (typeref->kind)
	{
	case type_void:
		return &_void_type;
	case type_int:
		return &_int_type;
	case type_struct:

		break;
	}

	ast_type_spec_t* exist = idm_find_tag(_id_map, typeref->struct_spec->name);
	if (exist)
	{
		if (exist->struct_spec->members && typeref->struct_spec->members)
		{
			//multiple definitions
			_report_err(typeref->tokens.start, ERR_TYPE_DUP,
				"redefinition of struct '%s'",
				typeref->struct_spec->name);
			return NULL;
		}

		if (!exist->struct_spec->members && typeref->struct_spec->members)
		{
			//update definition
			exist->struct_spec->members = typeref->struct_spec->members;
		}
		return exist;
	}

	idm_add_tag(_id_map, typeref);
	return typeref;
}

bool process_variable_declaration(ast_declaration_t* decl)
{
	ast_var_decl_t* var = &decl->data.var;
	//if (!var)
		//return true;

	ast_declaration_t* existing = idm_find_block_decl(_id_map, var->name, decl_var);
	if (existing)
	{
		_report_err(decl->tokens.start, ERR_DUP_VAR, "var %s already declared at",
			ast_declaration_name(decl));
		return false;
	}

	idm_add_id(_id_map, decl);

/*	ast_var_decl_t* var = &decl->data.var;
	if (!var)
		return true;

	var_data_t* existing = var_cur_block_find(_var_set, var->name);

	if (existing)
	{
		_report_err(decl->tokens.start, ERR_DUP_VAR, "var %s already declared at",
			var->name);
		return false;
	}
	var_decl_stack_var(_var_set, var);*/
	return true;
}

bool process_declaration(ast_declaration_t* var)
{
	if (!var)
		return true;
	switch (var->kind)
	{
	case decl_var:
		return process_variable_declaration(var);
	case decl_func:
		return process_function_decl(var) != proc_decl_error;
	}
	return true;
}

bool process_variable_reference(ast_expression_t* expr)
{
	/*if (!expr)
		return true;

	var_data_t* existing = var_find(_var_set, expr->data.var_reference.name);
	if (!existing)
	{
		_report_err(expr->tokens.start, ERR_UNKNOWN_VAR, "var %s not defined",expr->data.var_reference.name);
		return false;
	}*/
	return true;
}

bool process_variable_assignment(ast_expression_t* expr)
{
	/*if (!expr)
		return true;
	var_data_t* existing = var_find(_var_set, expr->data.assignment.name);
	if (!existing)
	{
		_report_err(expr->tokens.start, ERR_UNKNOWN_VAR, "var %s not defined", expr->data.assignment.name);
		return false;
	}*/

	return true;
}

static ast_type_spec_t* _resolve_expr_type(ast_expression_t* expr)
{
	if (!expr)
		return NULL;

	switch (expr->kind)
	{
	case expr_postfix_op:
	case expr_unary_op:
		return _resolve_expr_type(expr->data.unary_op.expression);
	case expr_binary_op:
	{
		ast_type_spec_t* l = _resolve_expr_type(expr->data.binary_op.lhs);
		ast_type_spec_t* r = _resolve_expr_type(expr->data.binary_op.rhs);
		if (l == r)
			return r;
		//if (!process_expression(expr->data.binary_op.lhs))
		//	return false;
		//return process_expression(expr->data.binary_op.rhs);
		break;
	}
	case expr_int_literal:
		return &_int_type;
		break;
	case expr_assign:
		return _resolve_expr_type(expr->data.assignment.target);
	case expr_identifier:
	{
		ast_declaration_t* decl = idm_find_decl(_id_map, expr->data.var_reference.name, decl_var);
		if (decl)
			return _resolve_type(decl->data.var.type);
		return NULL;
	}
	
	case expr_condition:
		//if (!process_expression(expr->data.condition.cond))
		//	return false;
		//if (!process_expression(expr->data.condition.true_branch))
		//	return false;
		//return process_expression(expr->data.condition.false_branch);
		break;
	case expr_func_call:
	{
		ast_declaration_t* decl = idm_find_decl(_id_map, expr->data.var_reference.name, decl_func);
		if (decl)
			return _resolve_type(decl->data.func.return_type);
		return NULL;
	}
	case expr_null:
		break;
	}
	return NULL;
}

bool process_func_call(ast_expression_t* expr)
{	
	if (!expr)
		return true;

	ast_declaration_t* decl = idm_find_decl(_id_map, expr->data.func_call.name, decl_func);
	if (!decl)
	{
		_report_err(expr->tokens.start,
			ERR_UNKNOWN_FUNC, 
			"function '%s' not defined", 
			expr->data.func_call.name);
		return false;
	}

	if (expr->data.func_call.param_count != decl->data.func.param_count)
	{
		_report_err(expr->tokens.start,
			ERR_INVALID_PARAMS,
			"incorrect number of params in call to function '%s'. Expected %d",
			expr->data.func_call.name, decl->data.func.param_count);
		return false;
	}

	/*
	Compare param types
	*/

	ast_expression_t* call_param = expr->data.func_call.params;
	ast_declaration_t* func_param = decl->data.func.params;

	int p_count = 1;
	while (call_param && func_param)
	{
		if (!process_expression(call_param))
			return false;

		if (func_param->data.var.type != _resolve_expr_type(call_param))
		{
			_report_err(expr->tokens.start,
				ERR_INVALID_PARAMS,
				"conflicting type in param %d of call to function '%s'. Expected '%s'",
				p_count, ast_declaration_name(decl), ast_type_name(func_param->data.var.type));
			return false;
		}

		call_param = call_param->next;
		func_param = func_param->next;
		p_count++;
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
		case expr_identifier:
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
		return process_declaration(block->decl);
	}	
	return false;
}

bool process_block_list(ast_block_item_t* blocks)
{
	idm_enter_block(_id_map);
	ast_block_item_t* block = blocks;
	while (block)
	{
		if (!process_block_item(block))
		{
			idm_leave_block(_id_map);
			return false;
		}
		block = block->next;
	}
	idm_leave_block(_id_map);
	return true;
}

bool process_for_statement(ast_statement_t* smnt)
{
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
	return true;
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
	{
		idm_enter_block(_id_map);
		bool result = process_for_statement(smnt);
		idm_leave_block(_id_map);
		return result;
	}
	case smnt_if:
		if (!process_expression(smnt->data.if_smnt.condition))
			return false;
		if (!process_statement(smnt->data.if_smnt.true_branch))
			return false;
		if (!process_statement(smnt->data.if_smnt.false_branch))
			return false;
		break;
	case smnt_compound:
		return process_block_list(smnt->data.compound.blocks);
		break;
	case smnt_return:
		return process_expression(smnt->data.expr);
	}

	return true;
}

bool process_function_definition(ast_function_decl_t* decl)
{
	return process_block_list(decl->blocks);
}

static bool _is_fn_definition(ast_declaration_t* decl)
{
	assert(decl->kind == decl_func);
	return decl->data.func.blocks != NULL;
}

bool resolve_function_decl_types(ast_declaration_t* decl)
{
	ast_function_decl_t* func = &decl->data.func;

	//return type
	ast_type_spec_t* ret_type = _resolve_type(func->return_type);
	if (!ret_type)
	{
		return false;
	}
	if (_is_fn_definition(decl) && 
		ret_type->kind != type_void && 
		ret_type->size == 0)
	{
		_report_err(decl->tokens.start, ERR_TYPE_INCOMPLETE,
			"function '%s' returns incomplete type '%s'",
			func->name, ret_type->struct_spec->name);
		return false;
	}
	func->return_type = ret_type;

	//parameter types
	ast_declaration_t* param = func->params;
	while (param)
	{
		if (param->data.var.type->kind == type_void)
		{
			_report_err(param->tokens.start, ERR_INVALID_PARAMS,
				"function param '%s' of void type",
				param->data.var.name);
			return false;
		}

		ast_type_spec_t* param_type = _resolve_type(param->data.var.type);
		if (!param_type)
			return false;
		
		if (_is_fn_definition(decl) && 
			param_type->kind != type_void &&
			param_type->size == 0)
		{
			_report_err(param->tokens.start, ERR_TYPE_INCOMPLETE,
				"function param '%s' returns incomplete type '%s'",
				param->data.var.name, param_type->struct_spec->name);
			return false;
		}
		
		param->data.var.type = param_type;
		param = param->next;
	}
	return true;
}

static bool _compare_func_decls(ast_declaration_t* exist, ast_declaration_t* func)
{
	if (exist->data.func.return_type != _resolve_type(func->data.func.return_type))
	{
		_report_err(func->tokens.start,
			ERR_INVALID_PARAMS,
			"differing return type in declaration of function '%s'. Expected '%s'",
			ast_declaration_name(func), ast_type_name(exist->data.func.return_type));
		return false;
	}

	if (exist->data.func.param_count != func->data.func.param_count)
	{
		_report_err(func->tokens.start, 
			ERR_INVALID_PARAMS,
			"incorrect number of params in declaration of function '%s'. Expected %d",
			ast_declaration_name(func), exist->data.func.param_count);
		return false;
	}

	ast_declaration_t* p_exist = exist->data.func.params;
	ast_declaration_t* p_func = func->data.func.params;

	int p_count = 1;
	while (p_exist && p_func)
	{
		if (p_exist->data.var.type != _resolve_type(p_func->data.var.type))
		{
			_report_err(func->tokens.start,
				ERR_INVALID_PARAMS,
				"conflicting type in param %d of declaration of function '%s'. Expected '%s'",
				p_count, ast_declaration_name(func), ast_type_name(p_exist->data.var.type));
			return false;
		}

		p_exist = p_exist->next;
		p_func = p_func->next;
		p_count++;
	}

	return p_exist == NULL && p_func == NULL;
}

proc_decl_result process_function_decl(ast_declaration_t* decl)
{
	const char* name = decl->data.func.name;

	ast_declaration_t* var = idm_find_decl(_id_map, name, decl_var);
	if (var)
	{
		_report_err(decl->tokens.start, ERR_DUP_SYMBOL,
			"declaration of function '%s' shadows variable at",
			name);
		return proc_decl_error;
	}

	ast_declaration_t* exist = idm_find_decl(_id_map, name, decl_func);
	if (exist)
	{
		if (_is_fn_definition(exist) && _is_fn_definition(decl))
		{
			//multiple definition
			_report_err(decl->tokens.start, ERR_DUP_SYMBOL,
				"redefinition of function '%s'", name);
			return proc_decl_error;
		}

		if (!_compare_func_decls(exist, decl))
			return proc_decl_error;

		if (!_is_fn_definition(exist) && _is_fn_definition(decl))
		{
			//update definition
			if (!resolve_function_decl_types(decl))
				return proc_decl_error;

			idm_update_decl(_id_map, decl);
		}
		//return proc_decl_ignore;
	}
	else
	{
		if (!resolve_function_decl_types(decl))
			return proc_decl_error;
		idm_add_id(_id_map, decl);
	}

	return _is_fn_definition(decl) ? proc_decl_new_def : proc_decl_ignore;
}

/*
Multiple declarations are allowed
Only a single definition is permitted
Type cannot change
*/
proc_decl_result process_global_var_decl(ast_declaration_t* decl)
{
	const char* name = decl->data.var.name;

	ast_declaration_t* fn = idm_find_decl(_id_map, name, decl_func);
	if (fn)
	{
		_report_err(decl->tokens.start, ERR_DUP_SYMBOL,
			"global var declaration of '%s' shadows function at",
			name);
		return proc_decl_error;
	}

	ast_declaration_t* exist = idm_find_decl(_id_map, name, decl_var);
	if (exist)
	{
		//multiple declarations are allowed
		if (exist->data.var.type != _resolve_type(decl->data.var.type))
		{
			//different types
			_report_err(decl->tokens.start, ERR_DUP_SYMBOL,
				"incompatible redeclaration of global var '%s'",
				name);
			return proc_decl_error;
		}

		if (exist->data.var.expr && decl->data.var.expr)
		{
			//multiple definitions
			_report_err(decl->tokens.start, ERR_DUP_SYMBOL,
				"redefinition of global var '%s'",
				name);
			return proc_decl_error;
		}

		if (!exist->data.var.expr && decl->data.var.expr)
		{
			//update definition
			exist->data.var.expr = decl->data.var.expr;
		}
		return proc_decl_ignore;
	}

	ast_type_spec_t* type = _resolve_type(decl->data.var.type);
	if (!type)
	{
		return proc_decl_error;
	}

	if (type->size == 0)
	{
		_report_err(decl->tokens.start, ERR_TYPE_INCOMPLETE,
			"global var '%s' uses incomplete type '%s'",
			name, type->struct_spec->name);
		return proc_decl_error;
	}

	decl->data.var.type = type;
	idm_add_id(_id_map, decl);
	return proc_decl_new_def;
}

valid_trans_unit_t* validate_tl(ast_trans_unit_t* tl)
{
	_id_map = idm_create();

	ast_declaration_t* vars = NULL;
	ast_declaration_t* fns = NULL;
	
	ast_declaration_t* decl = tl->decls;
	while (decl)
	{
		ast_declaration_t* next = decl->next;
		if (decl->kind == decl_var)
		{
			proc_decl_result result = process_global_var_decl(decl);

			if (result == proc_decl_error)
				return NULL;
			if (result == proc_decl_new_def)
			{
				decl->next = vars;
				vars = decl;
			}
		}
		else if (decl->kind == decl_func)
		{
			proc_decl_result result = process_function_decl(decl);
			if (result == proc_decl_error)
				return NULL;
			if (result == proc_decl_new_def)
			{
				decl->next = fns;
				fns = decl;
			}

			if (decl->data.func.blocks)
			{
				idm_enter_function(_id_map, &decl->data.func);
				process_function_definition(&decl->data.func);
				idm_leave_function(_id_map);
			}
		}
		else if (decl->kind == decl_type)
		{
			_resolve_type(&decl->data.type);
		}
		decl = next;
	}

	valid_trans_unit_t* result = (valid_trans_unit_t*)malloc(sizeof(valid_trans_unit_t));
	memset(result, 0, sizeof(valid_trans_unit_t));
	result->ast = tl;
	result->functions = fns;
	result->variables = vars;
	idm_destroy(_id_map);

	return result;
}
