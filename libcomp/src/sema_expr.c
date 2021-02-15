#include "sema_internal.h"
#include "std_types.h"
#include "diag.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

static expr_result_t _report_err(ast_expression_t* expr, int err, const char* format, ...)
{
	char buff[512];

	va_list args;
	va_start(args, format);
	vsnprintf(buff, 512, format, args);
	va_end(args);

	diag_err(expr->tokens.start, err, buff);

	expr_result_t result;
	memset(&result, 0, sizeof(expr_result_t));
	result.failure = true;
	return result;
}

static expr_result_t _process_array_subscript_binary_op(ast_expression_t* expr)
{
	// lhs[rhs]
	expr_result_t result;
	memset(&result, 0, sizeof(expr_result_t));

	expr_result_t lhs_result = sema_process_expression(expr->data.binary_op.lhs);
	if (lhs_result.failure)
		return lhs_result;

	if (lhs_result.result_type->kind != type_ptr)
		return _report_err(expr, ERR_INCOMPATIBLE_TYPE, "[] operator: expected pointer type");

	expr_result_t rhs_result = sema_process_expression(expr->data.binary_op.rhs);
	if (rhs_result.failure)
		return rhs_result;

	if(!ast_type_is_int(rhs_result.result_type))
		return _report_err(expr, ERR_INCOMPATIBLE_TYPE, "[] operator: subscript must be an integer type");

	//result is the type pointed to by lhs
	result.result_type = lhs_result.result_type->data.ptr_type;
	return result;
}

static expr_result_t _process_member_access_binary_op(ast_expression_t* expr)
{
	expr_result_t result;
	memset(&result, 0, sizeof(expr_result_t));

	// a.b (lhs.rhs)
	// a->b (lhs->rhs)
	expr_result_t lhs_result = sema_process_expression(expr->data.binary_op.lhs);
	if (lhs_result.failure)
		return lhs_result;

	ast_type_spec_t* user_type = NULL; //actual struct or union type the operator is applied to

	if (expr->data.binary_op.operation == op_ptr_member_access)
	{
		//ptr member access lhs must resolve to a pointer type
		if (lhs_result.result_type->kind != type_ptr)
			return _report_err(expr, ERR_INCOMPATIBLE_TYPE, "-> operator: expected pointer type");

		//type pointed to must be a user type (struct or union)
		if (lhs_result.result_type->data.ptr_type->kind != type_user)
			return _report_err(expr, ERR_INCOMPATIBLE_TYPE, "-> operator: expected user type");

		user_type = lhs_result.result_type->data.ptr_type;
	}
	else
	{
		//lhs must be a user type (struct or union)
		if (lhs_result.result_type->kind != type_user)
			return _report_err(expr, ERR_INCOMPATIBLE_TYPE, ". operator: expected user type");

		user_type = lhs_result.result_type;
	}

	//rhs must be an identifier
	if (expr->data.binary_op.rhs->kind != expr_identifier)
	{
		return _report_err(expr, ERR_INCOMPATIBLE_TYPE,
			"%s operator: operand must be an identifier", 
			expr->data.binary_op.operation == op_ptr_member_access ? "->" : ".");
	}

	ast_struct_member_t* member = ast_find_struct_member(user_type->data.user_type_spec, expr->data.binary_op.rhs->data.identifier.name);
	if (member == NULL)
	{
		return _report_err(expr, ERR_UNKNOWN_MEMBER_REF,
			"%s is not a member of %s",
			expr->data.binary_op.rhs->data.identifier.name,
			ast_type_name(user_type));
	}

	result.result_type = member->type_ref->spec;
	return result;
}

static expr_result_t _process_assignment(ast_expression_t* expr)
{
	ast_expression_t* target = expr->data.binary_op.lhs;
	ast_expression_t* source = expr->data.binary_op.rhs;

	expr_result_t target_result = sema_process_expression(target);
	if (target_result.failure)
		return target_result;

	expr_result_t result = sema_process_expression(source);
	if (result.failure)
		return result;

	if (result.result_type->kind == type_func_sig)
	{
		//implicit conversion to function pointer
		result.result_type = ast_make_ptr_type(result.result_type);
	}

	if (source->kind == expr_int_literal)
	{
		if (target_result.result_type->kind == type_ptr)
		{
			if (source->data.int_literal.val.v.int64 != 0)
			{
				return _report_err(expr, ERR_INCOMPATIBLE_TYPE,
					"assignment to incompatible type. expected %s",
					ast_type_name(target_result.result_type));
			}
		}
		else if (!int_val_will_fit(&source->data.int_literal.val, target_result.result_type))
		{
			return _report_err(expr, ERR_INCOMPATIBLE_TYPE,
				"assignment to incompatible type. expected %s",
				ast_type_name(target_result.result_type));
		}
	}
	else if (!sema_can_convert_type(target_result.result_type, result.result_type))
	{
		return _report_err(expr, ERR_INCOMPATIBLE_TYPE,
			"assignment to incompatible type. expected %s",
			ast_type_name(target_result.result_type));
	}

	return target_result;
}

static expr_result_t _process_binary_op(ast_expression_t* expr)
{
	expr_result_t result;
	memset(&result, 0, sizeof(expr_result_t));

	switch (expr->data.binary_op.operation)
	{
	case op_assign:
	case op_mul_assign:
	case op_div_assign:
	case op_mod_assign:
	case op_add_assign:
	case op_sub_assign:
	case op_left_shift_assign:
	case op_right_shift_assign:
	case op_and_assign:
	case op_xor_assign:
	case op_or_assign:
		return _process_assignment(expr);
	case op_member_access:
	case op_ptr_member_access:
		return _process_member_access_binary_op(expr);
	case op_array_subscript:
		return _process_array_subscript_binary_op(expr);
	}

	expr_result_t lhs_result = sema_process_expression(expr->data.binary_op.lhs);
	if (lhs_result.failure)
		return lhs_result;

	expr_result_t rhs_result = sema_process_expression(expr->data.binary_op.rhs);
	if (rhs_result.failure)
		return rhs_result;

	result.result_type = int32_type_spec;

	return result;
}

static expr_result_t _process_unary_op(ast_expression_t* expr)
{
	expr_result_t result = sema_process_expression(expr->data.unary_op.expression);

	if (result.failure)
		return result;

	switch (expr->data.unary_op.operation)
	{
	case op_address_of:
	{
		ast_type_spec_t* ptr_type = ast_make_ptr_type(result.result_type);
		result.result_type = ptr_type;
		break;
	}
	case op_dereference:
		if (result.result_type->kind != type_ptr)
			return _report_err(expr, ERR_INCOMPATIBLE_TYPE, "dereference operator: expected pointer type");
		result.result_type = result.result_type->data.ptr_type;
		break;
	case op_negate:
		if (!ast_type_is_int(result.result_type))
			return _report_err(expr, ERR_INCOMPATIBLE_TYPE, "negation operator: expected integer type");
		break;
	}
	
	
	return result;
}

expr_result_t sema_process_int_literal(ast_expression_t* expr)
{
	expr_result_t result;
	memset(&result, 0, sizeof(expr_result_t));

	ast_type_spec_t* type = int_val_smallest_size(&expr->data.int_literal.val);
		
	if (type)
	{
		expr->data.int_literal.val.is_signed = ast_type_is_signed_int(type);
		result.result_type = type;
		expr->data.int_literal.type = type;
	}
	else
	{
		result.failure = true;
	}

	return result;
}

static expr_result_t _process_str_literal(ast_expression_t* expr)
{
	expr_result_t result;
	memset(&result, 0, sizeof(expr_result_t));

	//set the label to use in asm
	expr->data.str_literal.label = idm_add_string_literal(sema_id_map(), expr->data.str_literal.value);
	result.result_type = ast_make_ptr_type(int8_type_spec);
	return result;
}

static expr_result_t _process_variable_reference(ast_expression_t* expr)
{
	expr_result_t result;
	memset(&result, 0, sizeof(expr_result_t));

	ast_declaration_t* decl = idm_find_decl(sema_id_map(), expr->data.identifier.name);
	if (decl)
	{
		expr->data.identifier.decl = decl;
		if (decl->kind == decl_var)
		{
			if (decl->type_ref->spec->size == 0)
			{
				return _report_err(expr, ERR_TYPE_INCOMPLETE,
					"var %s is of incomplete type",
					ast_declaration_name(decl));
			}
			result.result_type = decl->type_ref->spec;
			return result;
		}
		else if (decl->kind == decl_func)
		{
			//return function type
			result.result_type = decl->type_ref->spec;
			return result;
		}
	}

	//enum value?
	int_val_t* enum_val = idm_find_enum_val(sema_id_map(), expr->data.identifier.name);
	if (enum_val)
	{
		ast_destroy_expression_data(expr);
		expr->kind = expr_int_literal;
		expr->data.int_literal.val = *enum_val;
		return sema_process_expression(expr);
	}
	
	return _report_err(expr, ERR_UNKNOWN_IDENTIFIER,
		"identifier '%s' not defined",
		expr->data.identifier.name);
}

static expr_result_t _process_condition(ast_expression_t* expr)
{
	expr_result_t result = sema_process_expression(expr->data.condition.cond);
	if (result.failure)
		return result;

	if (expr->data.condition.true_branch)
	{
		result = sema_process_expression(expr->data.condition.true_branch);
		if (result.failure)
			return result;
	}

	if (expr->data.condition.false_branch)
	{
		result = sema_process_expression(expr->data.condition.false_branch);
		if (result.failure)
			return result;
	}

	result.result_type = int32_type_spec;
	return result;
}

static expr_result_t _process_func_call(ast_expression_t* expr)
{
	expr_result_t result;
	memset(&result, 0, sizeof(expr_result_t));

	expr_result_t target_result = sema_process_expression(expr->data.func_call.target);
	if (target_result.failure)
		return target_result;

	//target type - the type_spec of the function, or function pointer being called
	ast_type_spec_t* tt = target_result.result_type;

	//target is either an identifier referencing a function
	//or something which resolves to a function pointer
	ast_func_sig_type_spec_t* fsig = NULL;
	const char* fn_name = "anonymous";
	if (tt->kind == type_func_sig)
	{
		if (expr->data.func_call.target->kind != expr_identifier)
		{
			return _report_err(expr, ERR_INCOMPATIBLE_TYPE,
				"func call bad target");
		}
		fsig = tt->data.func_sig_spec;
		fn_name = expr->data.func_call.target->data.identifier.name;
	}
	else if (ast_type_is_fn_ptr(tt))
	{
		fsig = tt->data.ptr_type->data.func_sig_spec;
	}
	else
	{
		return _report_err(expr, ERR_INCOMPATIBLE_TYPE,
			"func call bad target");
	}

	if (expr->data.func_call.param_count != fsig->params->param_count)
	{
		if (expr->data.func_call.param_count < fsig->params->param_count ||
			!fsig->params->ellipse_param)
		{
			//not enough params, or no variable arg
			return _report_err(expr, ERR_INVALID_PARAMS,
				"incorrect number of params in call to function '%s'. Expected %d",
				fn_name, fsig->params->param_count);
		}
	}

	ast_func_call_param_t* call_param = expr->data.func_call.first_param;
	ast_func_param_decl_t* param_decl = fsig->params->first_param;
	int p_count = 1;
	while (param_decl)
	{
		assert(call_param);

		expr_result_t param_result = sema_process_expression(call_param->expr);
		if (param_result.failure)
			return param_result;

		if (!sema_can_convert_type(param_decl->decl->type_ref->spec, param_result.result_type))
		{
			return _report_err(call_param->expr, ERR_INVALID_PARAMS,
				"conflicting type in param %d of call to function '%s'. Expected '%s'",
				p_count, fn_name, ast_type_name(param_decl->decl->type_ref->spec));
		}

		call_param->expr_type = param_result.result_type;

		param_decl = param_decl->next;
		call_param = call_param->next;
		p_count++;
	}

	if (call_param != NULL)
	{
		assert(fsig->params->ellipse_param);

		//... params
		while (call_param)
		{
			expr_result_t param_result = sema_process_expression(call_param->expr);
			if (param_result.failure)
				return param_result;

			call_param->expr_type = param_result.result_type;
			call_param = call_param->next;
			p_count++;
		}
	}
	expr->data.func_call.sema.target_type = tt;
	result.result_type = fsig->ret_type;
	return result;
}

static expr_result_t _process_sizeof(ast_expression_t* expr)
{
	expr_result_t result;
	memset(&result, 0, sizeof(expr_result_t));

	ast_type_spec_t* type;

	if (expr->data.sizeof_call.kind == sizeof_type)
	{
		sema_resolve_type_ref(expr->data.sizeof_call.data.type_ref);
		type = expr->data.sizeof_call.data.type_ref->spec;
	}
	else
	{
		expr_result_t expr_result = sema_process_expression(expr->data.sizeof_call.data.expr);
		if (expr_result.failure)
			return expr_result;
		type = expr_result.result_type;
	}

	if (!type || type->size == 0)
	{
		return _report_err(expr, ERR_UNKNOWN_TYPE,
			"could not determin type for sizeof call");
	}

	//change expression to represent a constant int and re-process
	ast_destroy_expression_data(expr);
	expr->kind = expr_int_literal;
	expr->data.int_literal.val = int_val_unsigned(type->size);
	return sema_process_expression(expr);
}

//todo
bool process_sizeof_expr(ast_expression_t* expr)
{
	return !_process_sizeof(expr).failure;
}

static expr_result_t _process_null(ast_expression_t* expr)
{
	expr;
	expr_result_t result;
	memset(&result, 0, sizeof(expr_result_t));

	result.result_type = void_type_spec;
	return result;
}

expr_result_t _process_cast(ast_expression_t* expr)
{
	expr_result_t result = sema_process_expression(expr->data.cast.expr);
	result.result_type = expr->data.cast.type_ref->spec;
	return result;
}

expr_result_t sema_process_expression(ast_expression_t* expr)
{
	expr_result_t result;
	memset(&result, 0, sizeof(expr_result_t));

	if (sema_is_const_int_expr(expr))
	{
		//we can evalulate to a constant
		sema_fold_const_int_expr(expr);
	}

	switch (expr->kind)
	{
	case expr_null:
		result = _process_null(expr);
		break;
	case expr_postfix_op:
	case expr_unary_op:
		result = _process_unary_op(expr);
		break;
	case expr_binary_op:
		result = _process_binary_op(expr);
		break;
	case expr_int_literal:
		result = sema_process_int_literal(expr);
		break;
	case expr_str_literal:
		result = _process_str_literal(expr);
		break;
	case expr_identifier:
		result = _process_variable_reference(expr);
		break;
	case expr_condition:
		result = _process_condition(expr);
		break;
	case expr_func_call:
		result = _process_func_call(expr);
		break;
	case expr_sizeof:
		result = _process_sizeof(expr);
		break;
	case expr_cast:
		result = _process_cast(expr);
		break;
	}
	if (!result.failure)
		expr->sema.result_type = result.result_type;
	return result;
}