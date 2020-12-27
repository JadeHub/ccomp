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

	if (source->kind == expr_int_literal && target_result.result_type->kind != type_ptr)
	{

		if (!int_val_will_fit(&source->data.int_literal.val, target_result.result_type))
		{
			return _report_err(expr, ERR_INCOMPATIBLE_TYPE,
				"assignment to incompatible type. expected %s",
				ast_type_name(target_result.result_type));
		}
	}
	else if (target_result.result_type->kind != type_ptr)
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

static expr_result_t _process_binary_op(ast_expression_t* expr)
{
	expr_result_t result;
	memset(&result, 0, sizeof(expr_result_t));

	if (ast_is_assignment_op(expr->data.binary_op.operation))
	{
		return _process_assignment(expr);
	}
	else if (expr->data.binary_op.operation == op_member_access || expr->data.binary_op.operation == op_ptr_member_access)
	{
		return _process_member_access_binary_op(expr);
	}
	else if (expr->data.binary_op.operation == op_array_subscript)
	{
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
		result.result_type = ptr_type;// sema_resolve_type(ptr_type);
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

	ast_declaration_t* decl = idm_find_decl(sema_id_map(), expr->data.var_reference.name);
	if (decl && decl->kind == decl_var)
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

	//enum value?
	int_val_t* enum_val = idm_find_enum_val(sema_id_map(), expr->data.var_reference.name);
	if (enum_val)
	{
		ast_destroy_expression_data(expr);
		expr->kind = expr_int_literal;
		expr->data.int_literal.val = *enum_val;
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

	ast_declaration_t* decl = idm_find_decl(sema_id_map(), expr->data.var_reference.name);

	if (!decl || decl->kind != decl_func)
	{
		return _report_err(expr, ERR_UNKNOWN_FUNC,
			"function '%s' not defined",
			expr->data.func_call.name);
	}

	if (expr->data.func_call.param_count != decl->data.func.param_count)
	{
		if (expr->data.func_call.param_count < decl->data.func.param_count ||
			!decl->data.func.ellipse_param)
		{
			//not enough params, or no variable arg
			return _report_err(expr, ERR_INVALID_PARAMS,
				"incorrect number of params in call to function '%s'. Expected %d",
				expr->data.func_call.name, decl->data.func.param_count);
		}
	}

	ast_func_call_param_t* call_param = expr->data.func_call.first_param;
	ast_func_param_decl_t* param_decl = decl->data.func.first_param;
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
				p_count, ast_declaration_name(decl), ast_type_name(param_decl->decl->type_ref->spec));
		}

		call_param->expr_type = param_result.result_type;

		param_decl = param_decl->next;
		call_param = call_param->next;
		p_count++;
	}

	if (call_param != NULL)
	{
		assert(decl->data.func.ellipse_param);

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
	expr->data.func_call.func_decl = decl;
	result.result_type = decl->type_ref->spec;
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
		return sema_process_int_literal(expr);
	case expr_str_literal:
		return _process_str_literal(expr);
	//case expr_assign:
		//return _process_assignment(expr);
	case expr_identifier:
		return _process_variable_reference(expr);
	case expr_condition:
		return _process_condition(expr);
	case expr_func_call:
		return _process_func_call(expr);
	case expr_sizeof:
		return _process_sizeof(expr);
	case expr_cast:
		return _process_cast(expr);
	
	}

	return result;
}