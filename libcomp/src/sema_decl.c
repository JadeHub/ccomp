#include "sema_internal.h"

#include "diag.h"

#include <string.h>
#include <assert.h>

proc_decl_result sema_process_global_variable_declaration(ast_declaration_t* decl)
{
	ast_declaration_t* exist = idm_find_decl(sema_id_map(), decl->name);
	if (exist && exist->kind == decl_func)
	{
		diag_err(decl->tokens.start, ERR_DUP_SYMBOL,
			"global var declaration of '%s' shadows function at",
			decl->name);
		return proc_decl_error;
	}

	if (!sema_resolve_type_ref(decl->type_ref))
	{
		return proc_decl_error;
	}

	ast_type_spec_t* type = decl->type_ref->spec;

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
			sema_process_expression(exist->data.var.init_expr); //?
		}
		return proc_decl_ignore;
	}

	if (type->size == 0)
	{
		diag_err(decl->tokens.start, ERR_TYPE_INCOMPLETE,
			"global var '%s' uses incomplete type '%s'",
			decl->name, ast_type_name(decl->type_ref->spec));
		return proc_decl_error;
	}

	if (decl->data.var.init_expr)
	{
		if (sema_process_expression(decl->data.var.init_expr).failure)
			return false;

		ast_type_spec_t* var_type = decl->type_ref->spec;

		if (decl->data.var.init_expr->kind == expr_int_literal)
		{
			if (!int_val_will_fit(&decl->data.var.init_expr->data.int_literal.val, var_type))
			{
				//sema_report_type_conversion_error(decl->data.var.init_expr, &decl->data.var.init_expr->data.int_literal.val, var_type, "assignment");
				diag_err(decl->data.var.init_expr->tokens.start,
					ERR_INCOMPATIBLE_TYPE,
					"assignment to incompatible type. expected %s",
					ast_type_name(var_type));
				return proc_decl_error;
			}
		}
		else if (decl->data.var.init_expr->kind != expr_str_literal)
		{
			diag_err(decl->tokens.start, ERR_INITIALISER_NOT_CONST,
				"global var '%s' initialised with non-const integer expression", decl->name);
			return proc_decl_error;
		}
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
			"variable %s already declared at",
			ast_declaration_name(decl));
		return false;
	}

	if (!sema_resolve_type_ref(decl->type_ref) || decl->type_ref->spec->size == 0)
	{
		diag_err(decl->tokens.start, ERR_TYPE_INCOMPLETE,
			"var %s is of incomplete type",
			ast_declaration_name(decl));
		return false;
	}

	if (!sema_process_array_dimentions(decl))
		return false;

	if (decl->sema.alloc_size == 0)
		return false;

	if (decl->data.var.init_expr)
	{
		expr_result_t result;

		if (ast_type_is_struct_union(decl->type_ref->spec) && decl->data.var.init_expr->kind == expr_struct_init)
		{
			result = sema_process_struct_union_init_expression(decl->data.var.init_expr, decl->type_ref->spec);
		}
		else
		{
			result = sema_process_expression(decl->data.var.init_expr);
		}
		if (result.failure)
			return false;

		if (result.result_type->kind == type_func_sig)
		{
			//implicit conversion to function pointer
			result.result_type = ast_make_ptr_type(result.result_type);
		}

		ast_type_spec_t* init_type = result.result_type;

		if (!sema_can_convert_type(decl->type_ref->spec, init_type))
		{
			diag_err(decl->data.var.init_expr->tokens.start,
				ERR_INCOMPATIBLE_TYPE,
				"assignment to incompatible type. expected %s",
				ast_type_name(decl->type_ref->spec));
			return false;
		}
	}

	idm_add_decl(sema_id_map(), decl);
	sema_get_cur_fn_ctx()->decl->data.func.sema.required_stack_size += decl->sema.alloc_size;

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

	//return type
	fsig->ret_type = sema_resolve_type(fsig->ret_type, decl->tokens.start);
	if (!fsig->ret_type)
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

	//parameter types
	ast_func_param_decl_t* param = fsig->params->first_param;
	while (param)
	{
		if (param->decl->type_ref->spec->kind == type_void)
		{
			diag_err(param->decl->tokens.start, ERR_INVALID_PARAMS,
				"function param '%s' of void type",
				param->decl->name);
			return false;
		}

		if (!sema_resolve_type_ref(param->decl->type_ref))
			return false;

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
			"use of ellipse parameter requires at least one other parameter",
			name);
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
			//exist->data.func
			idm_update_decl(sema_id_map(), decl);
		}
	}
	else
	{
		idm_add_decl(sema_id_map(), decl);
	}

	return _is_fn_definition(decl) ? proc_decl_new_def : proc_decl_ignore;
}
