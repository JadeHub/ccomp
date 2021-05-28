#include "sema_internal.h"

#include "diag.h"

#include <string.h>
#include <assert.h>

expr_result_t sema_process_compound_init(ast_expression_t* expr, ast_type_spec_t* type);

bool sema_process_array_compound_init(ast_expression_t* expr, ast_type_spec_t* array_spec)
{
	ast_type_spec_t* elem_type = sema_resolve_type(array_spec->data.array_spec->element_type, expr->tokens.start);
	if (!elem_type)
		return false;
	array_spec->data.array_spec->element_type = elem_type;

	ast_compound_init_item_t* init_item = expr->data.compound_init.item_list;
	size_t elem_count = 0;
	while (init_item)
	{
		if (init_item->expr->kind == expr_compound_init)
		{
			if (sema_process_compound_init(init_item->expr, elem_type).failure)
				return false;
		}
		else
		{
			expr_result_t init_result = sema_process_expression(init_item->expr);
			if (init_result.failure)
				return false;

			if (!sema_can_convert_type(elem_type, init_result.result_type))
			{
				sema_report_type_conversion_error(init_item->expr, elem_type, init_result.result_type,
					"initialisation of array element");
				return false;
			}
		}
		elem_count++;
		init_item = init_item->next;
	}

	if (array_spec->size == 0)
	{
		array_spec->data.array_spec->sema.array_sz = elem_count; //not sure this is right when elements are themselves arrays
		array_spec->size = elem_count * elem_type->size;
	}
	else
	{
		if (elem_count != array_spec->data.array_spec->sema.array_sz)
		{
			diag_err(expr->tokens.start, ERR_INVALID_INIT,
				"incorrect number of items in array initialisation");
			return false;
		}
	}

	return true;
}

bool sema_process_struct_compound_init(ast_expression_t* expr, ast_type_spec_t* user_type)
{
	ast_struct_member_t* member = user_type->data.user_type_spec->data.struct_members;
	ast_compound_init_item_t* init_item = expr->data.compound_init.item_list;

	while (member)
	{
		if (init_item == NULL)
		{
			diag_err(expr->tokens.start, ERR_SYNTAX, "Incorrect number of init expressions for compound type");
			return false;
		}

		expr_result_t init_result;

		if (init_item->expr->kind == expr_compound_init)
		{
			init_result = sema_process_compound_init(init_item->expr, member->decl->type_ref->spec);
		}
		else
		{
			init_result = sema_process_expression(init_item->expr);
		}

		if (init_result.failure)
			return false;

		if (!sema_can_perform_assignment(member->decl->type_ref->spec, init_item->expr))
		{
			sema_report_type_conversion_error(init_item->expr, member->decl->type_ref->spec, init_result.result_type,
				"initialisation of member '%s'", member->decl->name);
			return false;
		}

		init_item->sema.member = member;

		member = member->next;
		init_item = init_item->next;
	}

	if (init_item)
	{
		diag_err(expr->tokens.start, ERR_SYNTAX, "Incorrect number of init expressions for compound type");
		return false;
	}

	return true;
}

expr_result_t sema_process_compound_init(ast_expression_t* expr, ast_type_spec_t* type)
{
	expr_result_t result;
	memset(&result, 0, sizeof(expr_result_t));

	if (ast_type_is_struct_union(type))
	{
		result.failure = sema_process_struct_compound_init(expr, type) == false;
	}
	else if (ast_type_is_array(type))
	{
		result.failure = !sema_process_array_compound_init(expr, type);

	}
	else
	{
		diag_err(expr->tokens.start, ERR_TYPE_INCOMPLETE,
			"compound initialiser requires array or sruct / union type");
		result.failure = true;
		return result;
	}

	if (!result.failure)
	{
		result.result_type = type;
		expr->sema.result.type = type;
	}

	return result;
}

static bool _process_global_var_init(ast_declaration_t* decl)
{
	if (decl->data.var.init_expr)
	{
		ast_expression_t* expr = decl->data.var.init_expr;
		if (expr->kind == expr_compound_init)
		{
			if (sema_process_compound_init(expr, decl->type_ref->spec).failure)
				return false;
		}
		else
		{
			if (sema_process_expression(expr).failure)
				return false;

			ast_type_spec_t* var_type = decl->type_ref->spec;

			if(ast_type_is_ptr(var_type))
			{ 
				if (!sema_is_const_pointer(expr) && expr->kind != expr_int_literal)
				{
					diag_err(expr->tokens.start,
						ERR_INITIALISER_NOT_CONST,
						"global pointer '%s' must be initialised with a const value",
						ast_type_name(var_type));
					return false;
				}
			}
			else if (expr->kind == expr_int_literal)
			{
				if (!int_val_will_fit(&expr->data.int_literal.val, var_type))
				{
					//sema_report_type_conversion_error(decl->data.var.init_expr, &decl->data.var.init_expr->data.int_literal.val, var_type, "assignment");
					diag_err(expr->tokens.start,
						ERR_INCOMPATIBLE_TYPE,
						"assignment to incompatible type. expected %s",
						ast_type_name(var_type));
					return false;
				}
			}
			else
			{
				diag_err(decl->tokens.start, ERR_INITIALISER_NOT_CONST,
					"global var '%s' initialised with non-const integer expression", decl->name);
				return false;
			}
		}
	}
	return true;
}

proc_decl_result sema_process_global_variable_declaration(ast_declaration_t* decl)
{
	if (!sema_resolve_type_ref(decl->type_ref))
		return proc_decl_error;

	ast_type_spec_t* type = decl->type_ref->spec;
	ast_declaration_t* exist = idm_find_decl(sema_id_map(), decl->name);

	if (exist && exist->kind == decl_func)
	{
		diag_err(decl->tokens.start, ERR_DUP_SYMBOL,
			"global var declaration of '%s' shadows function at",
			decl->name);
		return proc_decl_error;
	}

	if (exist && exist->kind == decl_var)
	{
		//multiple declarations are allowed
		if (!type || !sema_is_same_type(exist->type_ref->spec, type))
		{
			//different types
			diag_err(decl->tokens.start, ERR_DUP_SYMBOL,
				"incompatible redeclaration of global var '%s'",
				decl->name);
			return proc_decl_error;
		}

		if (exist->data.var.init_expr && decl->data.var.init_expr)
		{
			//multiple definitions
			diag_err(decl->tokens.start, ERR_DUP_SYMBOL,
				"redefinition of global var '%s'",
				decl->name);
			return proc_decl_error;
		}

		if (!exist->data.var.init_expr && decl->data.var.init_expr)
		{
			//update definition
			exist->data.var.init_expr = decl->data.var.init_expr;
			decl->data.var.init_expr = NULL;

			//process array dimentions?
			if(!_process_global_var_init(decl))
				return proc_decl_error;
		}
		return proc_decl_ignore;
	}

	if (!_process_global_var_init(decl))
		return proc_decl_error;

	if (decl->type_ref->spec->size == 0)
	{
		diag_err(decl->tokens.start, ERR_TYPE_INCOMPLETE,
			"global var '%s' uses incomplete type '%s'",
			decl->name, ast_type_name(decl->type_ref->spec));
		return proc_decl_error;
	}

	idm_add_decl(sema_id_map(), decl);
	return proc_decl_new_def;
}

bool sema_process_variable_declaration(ast_declaration_t* decl)
{
	ast_declaration_t* existing = idm_find_block_decl(sema_id_map(), decl->name);
	if (existing && existing->kind == decl_var)
	{
		diag_err(decl->tokens.start, ERR_DUP_VAR,
			"variable '%s' already declared at",
			ast_declaration_name(decl));
		return false;
	}

	if (!sema_resolve_type_ref(decl->type_ref))
	{
		diag_err(decl->tokens.start, ERR_TYPE_INCOMPLETE,
			"var '%s' is of incomplete type",
			ast_declaration_name(decl));
		return false;
	}

	if (decl->data.var.init_expr)
	{
		expr_result_t result;
		ast_expression_t* expr = decl->data.var.init_expr;

		if (expr->kind == expr_compound_init)
		{
			result = sema_process_compound_init(expr, decl->type_ref->spec);
		}
		else
		{
			result = sema_process_expression(decl->data.var.init_expr);
		}
		
		if (result.failure)
			return false;

		ast_type_spec_t* init_type = result.result_type;

		if(!sema_can_perform_assignment(decl->type_ref->spec, decl->data.var.init_expr))
		{
			sema_report_type_conversion_error(decl->data.var.init_expr,
				decl->type_ref->spec, init_type, "initialisation");
			return false;
		}
	}

	//check this here as the size may have been set while processing the init expression in the case of an array 'int []i = {1, 2}'
	if (decl->type_ref->spec->size == 0)
	{
		diag_err(decl->tokens.start, ERR_TYPE_INCOMPLETE,
			"var '%s' is of incomplete type",
			ast_declaration_name(decl));
		return false;
	}

	idm_add_decl(sema_id_map(), decl);
	sema_get_cur_fn_ctx()->decl->data.func.sema.required_stack_size += decl->type_ref->spec->size;

	return true;
}

bool sema_process_type_decl(ast_declaration_t* decl)
{
	if (!sema_resolve_type_ref(decl->type_ref))
		return false;

	if (strlen(decl->name))
	{
		ast_declaration_t* existing = idm_find_block_decl(sema_id_map(), decl->name);
		if (existing)
		{
			diag_err(decl->tokens.start, ERR_DUP_TYPE_DEF,
				"typedef forces redefinition of %s", decl->name);
			return false;
		}

		idm_add_decl(sema_id_map(), decl);
	}

	return true;
}

static bool _is_fn_definition(ast_declaration_t* decl)
{
	assert(decl->kind == decl_func);
	return decl->data.func.blocks != NULL;
}

bool resolve_function_decl_types(ast_declaration_t* decl)
{
	ast_func_sig_type_spec_t* fsig = decl->type_ref->spec->data.func_sig_spec;

	if (!sema_resolve_function_sig_types(fsig, decl->tokens.start))
		return false;

	if (_is_fn_definition(decl) &&
		fsig->ret_type->kind != type_void &&
		fsig->ret_type->size == 0)
	{
		diag_err(decl->tokens.start, ERR_TYPE_INCOMPLETE,
			"function '%s' returns incomplete type '%s'",
			decl->name, ast_type_name(fsig->ret_type));
		return false;
	}

	ast_func_param_decl_t* param = fsig->params->first_param;
	while (param)
	{
		ast_type_spec_t* param_type = param->decl->type_ref->spec;
		if (_is_fn_definition(decl) &&
			param_type->kind != type_void &&
			param_type->size == 0)
		{
			diag_err(param->decl->tokens.start, ERR_TYPE_INCOMPLETE,
				"function param '%s' of incomplete type '%s'",
				param->decl->name, param_type->data.user_type_spec->name);
			return false;
		}

		param = param->next;
	}

	return true;
}

proc_decl_result sema_process_function_decl(ast_declaration_t* decl)
{
	const char* name = decl->name;

	ast_declaration_t* exist = idm_find_decl(sema_id_map(), name);
	if (exist && exist->kind == decl_var)
	{
		diag_err(decl->tokens.start, ERR_DUP_SYMBOL,
			"declaration of function '%s' shadows variable at",
			name);
		return proc_decl_error;
	}

	assert(decl->type_ref->spec->kind == type_func_sig);

	ast_func_params_t* params = ast_func_decl_params(decl);
	if (params->ellipse_param && params->param_count == 0)
	{
		diag_err(decl->tokens.start, ERR_SYNTAX,
			"use of ellipse parameter in '%s' requires at least one other parameter",
			name);
		return proc_decl_error;
	}

	if (ast_type_is_array(ast_func_decl_return_type(decl)))
	{
		diag_err(decl->tokens.start, ERR_SYNTAX,
			"function '%s' may not return array type", name);
		return proc_decl_error;
	}

	if (!resolve_function_decl_types(decl))
		return proc_decl_error;

	if (exist)
	{
		if (_is_fn_definition(exist) && _is_fn_definition(decl))
		{
			//multiple definition
			diag_err(decl->tokens.start, ERR_DUP_SYMBOL,
				"redefinition of function '%s'", name);
			return proc_decl_error;
		}

		if (!sema_is_same_func_sig(exist->type_ref->spec->data.func_sig_spec, decl->type_ref->spec->data.func_sig_spec))
		{
			diag_err(decl->tokens.start, ERR_INVALID_PARAMS,
				"differing function signature in definition of '%s'", name);
			return proc_decl_error;
		}

		if (!_is_fn_definition(exist) && _is_fn_definition(decl))
		{
			//update definition
			idm_update_decl(sema_id_map(), decl);
		}
	}
	else
	{
		idm_add_decl(sema_id_map(), decl);
	}

	return _is_fn_definition(decl) ? proc_decl_new_def : proc_decl_ignore;
}
