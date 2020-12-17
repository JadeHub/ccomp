#include "sema_internal.h"
#include "std_types.h"
#include "diag.h"

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

	ast_struct_member_t* member = ast_find_struct_member(user_type->data.user_type_spec, expr->data.binary_op.rhs->data.var_reference.name);
	if (member == NULL)
	{
		return _report_err(expr, ERR_UNKNOWN_MEMBER_REF,
			"%s is not a member of %s",
			expr->data.binary_op.rhs->data.var_reference.name,
			ast_type_name(user_type));
	}

	result.result_type = member->type_ref->spec;
	return result;
}

static expr_result_t _process_binary_op(ast_expression_t* expr)
{
	expr_result_t result;
	memset(&result, 0, sizeof(expr_result_t));

	switch (expr->data.binary_op.operation)
	{
	case op_member_access:
		//fallthrough
	case op_ptr_member_access:
		return _process_member_access_binary_op(expr);
	case op_array_subscript:
		return _process_array_subscript_binary_op(expr);
	default:
		break;
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
		result.result_type = sema_resolve_type(ptr_type);
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

static expr_result_t _process_int_literal(ast_expression_t* expr)
{
	expr_result_t result;
	memset(&result, 0, sizeof(expr_result_t));

	ast_type_spec_t* type = int_val_smallest_size(&expr->data.int_literal.val);

	if (type)
	{
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

static expr_result_t _process_assignment(ast_expression_t* expr)
{
	expr_result_t target_result = sema_process_expression(expr->data.assignment.target);
	if (target_result.failure)
		return target_result;

	expr_result_t result = sema_process_expression(expr->data.assignment.expr);
	if (result.failure)
		return result;

	if (expr->data.assignment.expr->kind == expr_int_literal)
	{
		if (!int_val_will_fit(&expr->data.assignment.expr->data.int_literal.val, target_result.result_type))
		{
			return _report_err(expr, ERR_INCOMPATIBLE_TYPE,
				"assignment to incompatible type. expected %s",
				ast_type_name(target_result.result_type));
		}
	}
	else
	{
		if (!sema_can_convert_type(target_result.result_type, result.result_type))
		{
			return _report_err(expr, ERR_INCOMPATIBLE_TYPE,
				"assignment to incompatible type. expected %s",
				ast_type_name(target_result.result_type));
		}
	}

	return target_result;
}

static expr_result_t _process_variable_reference(ast_expression_t* expr)
{
	expr_result_t result;
	memset(&result, 0, sizeof(expr_result_t));

	ast_declaration_t* decl = idm_find_decl(sema_id_map(), expr->data.var_reference.name, decl_var);
	if (decl)
	{
		if (decl->data.var.type_ref->spec->size == 0)
		{
			return _report_err(expr, ERR_TYPE_INCOMPLETE,
				"var %s is of incomplete type",
				ast_declaration_name(decl));
		}
		result.result_type = decl->data.var.type_ref->spec;
		return result;
	}

	//enum value?
	decl = idm_find_decl(sema_id_map(), expr->data.var_reference.name, decl_const);
	if (decl)
	{
		ast_destroy_expression_data(expr);
		expr->kind = expr_int_literal;
		expr->data.int_literal = decl->data.const_val.expr->data.int_literal;
		return sema_process_expression(expr);
	}

	return _report_err(expr, ERR_UNKNOWN_VAR, "unknown var %s",
		expr->data.var_reference.name);
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

	ast_declaration_t* decl = idm_find_decl(sema_id_map(), expr->data.var_reference.name, decl_func);

	if (!decl)
	{
		return _report_err(expr, ERR_UNKNOWN_FUNC,
			"function '%s' not defined",
			expr->data.func_call.name);
	}

	if (expr->data.func_call.param_count != decl->data.func.param_count)
	{
		return _report_err(expr, ERR_INVALID_PARAMS,
			"incorrect number of params in call to function '%s'. Expected %d",
			expr->data.func_call.name, decl->data.func.param_count);
	}

	ast_expression_t* call_param = expr->data.func_call.params;
	ast_func_param_decl_t* func_param = decl->data.func.last_param;

	int p_count = 1;
	while (call_param && func_param)
	{
		expr_result_t param_result = sema_process_expression(call_param);
		if (param_result.failure)
			return param_result;

		if (!sema_can_convert_type(func_param->decl->data.var.type_ref->spec, param_result.result_type))
		{
			return _report_err(expr, ERR_INVALID_PARAMS,
				"conflicting type in param %d of call to function '%s'. Expected '%s'",
				p_count, ast_declaration_name(decl), ast_type_name(func_param->decl->data.var.type_ref->spec));
		}

		call_param = call_param->next;
		func_param = func_param->prev;
		p_count++;
	}
	expr->data.func_call.func_decl = decl;
	result.result_type = decl->data.func.return_type_ref->spec;
	return result;
}

static expr_result_t _process_sizeof(ast_expression_t* expr)
{
	expr_result_t result;
	memset(&result, 0, sizeof(expr_result_t));

	ast_type_spec_t* type;

	if (expr->data.sizeof_call.kind == sizeof_type)
	{
		type = sema_resolve_type(expr->data.sizeof_call.data.type);
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

expr_result_t sema_process_expression(ast_expression_t* expr)
{
	expr_result_t result;
	memset(&result, 0, sizeof(expr_result_t));

	if (sema_is_int_constant_expression(expr) && expr->kind != expr_int_literal)
	{
		//we can evalulate to a constant
		int_val_t val = sema_eval_constant_expr(expr);
		ast_destroy_expression_data(expr);
		memset(expr, 0, sizeof(ast_expression_t));
		expr->kind = expr_int_literal;
		expr->data.int_literal.val = val;
	}

	switch (expr->kind)
	{
	case expr_null:
		return _process_null(expr);
	case expr_postfix_op:
	case expr_unary_op:
		return _process_unary_op(expr);
	case expr_binary_op:
		return _process_binary_op(expr);
	case expr_int_literal:
		return _process_int_literal(expr);
	case expr_str_literal:
		return _process_str_literal(expr);
	case expr_assign:
		return _process_assignment(expr);
	case expr_identifier:
		return _process_variable_reference(expr);
	case expr_condition:
		return _process_condition(expr);
	case expr_func_call:
		return _process_func_call(expr);
	case expr_sizeof:
		return _process_sizeof(expr);
	
	}

	return result;
}