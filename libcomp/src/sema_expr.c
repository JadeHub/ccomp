#include "sema_internal.h"
#include "std_types.h"
#include "diag.h"

#include <libj/include/str_buff.h>

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

expr_result_t sema_report_type_conversion_error(ast_expression_t* expr, ast_type_spec_t* expected, ast_type_spec_t* actual, const char* format, ...)
{
	str_buff_t* sb = sb_create(128);

	sb_append(sb, "incompatible type in ");

	char buff[256];

	va_list args;
	va_start(args, format);
	vsnprintf(buff, 256, format, args);
	va_end(args);

	sb_append(sb, buff);
	sb_append(sb, " expected '");
	ast_type_spec_desc(sb, expected);
	sb_append(sb, "' found '");
	ast_type_spec_desc(sb, actual);
	sb_append_ch(sb, '\'');

	expr_result_t result = _report_err(expr, ERR_INCOMPATIBLE_TYPE, sb_str(sb));
	sb_destroy(sb);
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

	if(!ast_type_is_array(lhs_result.result_type) && !ast_type_is_ptr(lhs_result.result_type))
		return _report_err(expr, ERR_INCOMPATIBLE_TYPE, "[] operator: expected array or pointer type");

	expr_result_t rhs_result = sema_process_expression(expr->data.binary_op.rhs);
	if (rhs_result.failure)
		return rhs_result;

	if(!ast_type_is_int(rhs_result.result_type))
		return _report_err(expr, ERR_INCOMPATIBLE_TYPE, "[] operator: subscript must be an integer type");

	if (expr->data.binary_op.rhs->kind == expr_int_literal)
	{
		//bounds checks
		//ast_expression_t* int_expr = expr->data.binary_op.rhs;
		int_val_t* idx = &expr->data.binary_op.rhs->data.int_literal.val;
		if (idx->is_signed && idx->v.int64 < 0)
		{
			return _report_err(expr, ERR_ARRAY_BOUNDS, "negative array index");
		}

		if (ast_type_is_array(lhs_result.result_type) && idx->v.uint64 >= lhs_result.result_type->data.array_spec->sema.array_sz)
		{
			return _report_err(expr, ERR_ARRAY_BOUNDS, "array index %d exceeds size of %d",
				idx->v.uint64, lhs_result.result_type->data.array_spec->sema.array_sz);
		}
	}

	//result is the type pointed to by lhs
	if (ast_type_is_array(lhs_result.result_type))
		result.result_type = lhs_result.result_type->data.array_spec->element_type;
	else if (ast_type_is_ptr(lhs_result.result_type))
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

	result.result_type = member->decl->type_ref->spec;
	result.member = member;
	return result;
}

bool sema_is_const_pointer(ast_expression_t* expr)
{
	if (expr->kind == expr_str_literal)
		return true;

	if (expr->kind == expr_int_literal)
		return true;

	return expr->kind == expr_cast && 
		ast_type_is_ptr(expr->data.cast.type_ref->spec) &&
		sema_is_const_pointer(expr->data.cast.expr);
}

bool sema_is_null_ptr_expr(ast_expression_t* expr)
{
	return expr->kind == expr_cast &&
		ast_type_is_void_ptr(expr->data.cast.type_ref->spec) &&
		ast_expr_is_int_literal(expr->data.cast.expr, 0);
}

bool sema_can_perform_assignment(ast_type_spec_t* target, ast_expression_t* source)
{
	if (ast_type_is_ptr(target) && //when assigning to a pointer we accept...
		(source->kind == expr_int_literal || //a const int
		sema_is_const_pointer(source) || //a constant pointer
		ast_type_is_void_ptr(source->sema.result.type) // a void pointer
	))
	{
		return true;
	}

	ast_type_spec_t* src_type = source->sema.result.type;

	if (ast_type_is_fn_ptr(target) && src_type->kind == type_func_sig)
	{
		//implicit conversion to function pointer
		return sema_is_same_func_sig(target->data.ptr_type->data.func_sig_spec, src_type->data.func_sig_spec);
	}

	return sema_can_convert_type(target, source->sema.result.type);
}

static expr_result_t _process_assignment(ast_expression_t* expr)
{
	ast_expression_t* target = expr->data.binary_op.lhs;
	ast_expression_t* source = expr->data.binary_op.rhs;

	expr_result_t target_result = sema_process_expression(target);
	if (target_result.failure)
		return target_result;

	if (ast_type_is_array(target_result.result_type))
	{
		return _report_err(expr, ERR_INCOMPATIBLE_TYPE,
			"cannot assign to array type expression");
	}

	expr_result_t result = sema_process_expression(source);
	if (result.failure)
		return result;

	if (result.result_type->kind == type_func_sig)
	{
		//implicit conversion to function pointer
		result.result_type = ast_make_ptr_type(result.result_type);
	}

	if (!sema_can_perform_assignment(target_result.result_type, source))
	{
		sema_report_type_conversion_error(expr,
			target_result.result_type,
			result.result_type, "assignment");
		target_result.failure = true;
		return target_result;
	}

	return target_result;
}

static expr_result_t _process_logical_bin_op(ast_expression_t* expr)
{
	expr_result_t lhs_result = sema_process_expression(expr->data.binary_op.lhs);
	if (lhs_result.failure)
		return lhs_result;

	expr_result_t rhs_result = sema_process_expression(expr->data.binary_op.rhs);
	if (rhs_result.failure)
		return rhs_result;

	expr_result_t result;
	memset(&result, 0, sizeof(expr_result_t));

	result.result_type = int8_type_spec; //todo

	return result;
}

static expr_result_t _process_comparison_op(ast_expression_t* expr)
{
	expr_result_t lhs_result = sema_process_expression(expr->data.binary_op.lhs);
	if (lhs_result.failure)
		return lhs_result;

	expr_result_t rhs_result = sema_process_expression(expr->data.binary_op.rhs);
	if (rhs_result.failure)
		return rhs_result;

	expr_result_t result;
	memset(&result, 0, sizeof(expr_result_t));

	result.result_type = int8_type_spec; //todo

	return result;
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
	case op_eq:
	case op_neq:
	case op_lessthan:
	case op_lessthanequal:
	case op_greaterthan:
	case op_greaterthanequal:
		return _process_comparison_op(expr);

	case op_and:
	case op_or:
		return _process_logical_bin_op(expr);
	}

	expr_result_t lhs_result = sema_process_expression(expr->data.binary_op.lhs);
	if (lhs_result.failure)
		return lhs_result;

	expr_result_t rhs_result = sema_process_expression(expr->data.binary_op.rhs);
	if (rhs_result.failure)
		return rhs_result;

	//this is crap
	if (ast_type_is_ptr(lhs_result.result_type) && ast_type_is_int(rhs_result.result_type))
		result.result_type = lhs_result.result_type;
	else if (sema_is_same_type(lhs_result.result_type, rhs_result.result_type))
		result.result_type = rhs_result.result_type;
	else
		result.result_type = int32_type_spec; //todo
	

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
		if (result.member && ast_is_bit_field_member(result.member))
		{
			return _report_err(expr, ERR_INCOMPATIBLE_TYPE, "address of operator: cannot take address of bit field member");
		}

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
	expr->data.str_literal.sema.label = idm_add_string_literal(sema_id_map(), expr->data.str_literal.value);
	result.result_type = ast_make_ptr_type(int8_type_spec);
	return result;
}

static expr_result_t _process_identifier_reference(ast_expression_t* expr)
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

			if (decl->data.var.init_expr &&
				decl->data.var.init_expr->kind == expr_int_literal &&
				decl->type_ref->flags & TF_QUAL_CONST)
			{
				//expression references a constant variable (likely an enum value), convert the var reference expression into an int literal expression

				ast_destroy_expression_data(expr);
				expr->kind = expr_int_literal;
				expr->data.int_literal = decl->data.var.init_expr->data.int_literal;
				return sema_process_expression(expr);
			}

			return result;
		}
		else if (decl->kind == decl_func)
		{
			//return function type
			result.result_type = decl->type_ref->spec;
			return result;
		}
	}

	return _report_err(expr, ERR_UNKNOWN_IDENTIFIER,
		"identifier '%s' not defined",
		expr->data.identifier.name);
}

static expr_result_t _process_conditional_branches(ast_expression_t* expr, ast_type_spec_t* lhs_type, ast_type_spec_t* rhs_type)
{
	expr_result_t result;
	memset(&result, 0, sizeof(expr_result_t));

	if (lhs_type == rhs_type)
	{
		result.result_type = lhs_type;
		return result;
	}

	/*
	Only the following expressions are allowed as expression-true and expression-false
	- two expressions of any arithmetic type
	- two expressions of the same struct or union type
	- two expressions of void type
	- two expressions of pointer type, pointing to types that are compatible, ignoring cvr-qualifiers
	- one expression is a pointer and the other is the null pointer constant (such as NULL)
	- one expression is a pointer to object and the other is a pointer to void (possibly qualified)
	*/

	if (ast_type_is_int(lhs_type))
	{
		if (!ast_type_is_int(rhs_type))
		{
			return _report_err(expr->data.condition.false_branch,
				ERR_INCOMPATIBLE_TYPE,
				"arithmetic type expected");
		}

		//todo - which int type
		result.result_type = int32_type_spec;
	}
	else if (ast_type_is_struct_union(lhs_type))
	{
		if (lhs_type != rhs_type)
		{
			return _report_err(expr->data.condition.false_branch,
				ERR_INCOMPATIBLE_TYPE,
				"incompatible operand types, expected '%s'", lhs_type->data.user_type_spec->name);
		}
		result.result_type = lhs_type;
	}
	else if (lhs_type->kind == type_void)
	{
		if (lhs_type != rhs_type)
		{
			return _report_err(expr->data.condition.false_branch,
				ERR_INCOMPATIBLE_TYPE,
				"incompatible operand types, expected void");
		}
		result.result_type = lhs_type;
	}
	else if (ast_type_is_ptr(lhs_type))
	{
		if (!ast_type_is_ptr(rhs_type))
		{
			return _report_err(expr->data.condition.false_branch,
				ERR_INCOMPATIBLE_TYPE,
				"incompatible operand types, expected pointer");
		}

		result = _process_conditional_branches(expr, lhs_type->data.ptr_type, rhs_type->data.ptr_type);
		if (!result.failure)
			result.result_type = ast_make_ptr_type(result.result_type);
	}

	//todo

	result.failure = result.result_type == NULL;
	return result;
}

static expr_result_t _process_conditional(ast_expression_t* expr)
{
	expr_result_t result = sema_process_expression(expr->data.condition.cond);
	if (result.failure)
		return result;

	result = sema_process_expression(expr->data.condition.true_branch);
	if (result.failure)
		return result;
	ast_type_spec_t* lhs_type = expr->data.condition.true_branch->sema.result.type;

	result = sema_process_expression(expr->data.condition.false_branch);
	if (result.failure)
		return result;
	ast_type_spec_t* rhs_type = expr->data.condition.false_branch->sema.result.type;
	

	result = _process_conditional_branches(expr, lhs_type, rhs_type);



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
			return sema_report_type_conversion_error(expr, param_decl->decl->type_ref->spec, param_result.result_type,
				"param %d of call to function '%s'", p_count, fn_name);
		}

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
	if (result.failure)
		return result;

	ast_type_spec_t* target_type = sema_resolve_type(expr->data.cast.type_ref->spec, expr->tokens.start);
	if (target_type)
		result.result_type = target_type;
	else
		result.failure = true;
	return result;
}

expr_result_t sema_process_expression(ast_expression_t* expr)
{
	expr_result_t result;
	memset(&result, 0, sizeof(expr_result_t));

	if (sema_is_const_int_expr(expr))
	{
		//we can evalulate to a constant
		if (!sema_fold_const_int_expr(expr))
		{
			result.failure = true;
			return result;
		}
	}

	switch (expr->kind)
	{
	case expr_null:
		result = _process_null(expr);
		break;
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
		result = _process_identifier_reference(expr);
		break;
	case expr_condition:
		result = _process_conditional(expr);
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
	case expr_compound_init:
		assert(false);
		break;
	}
	if (!result.failure)
	{
		expr->sema.result.type = result.result_type;
		//expr->sema.result.array = result.array;
	}
	return result;
}