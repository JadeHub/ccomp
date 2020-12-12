#include "sema_internal.h"

bool sema_is_int_constant_expression(ast_expression_t* expr)
{
	switch (expr->kind)
	{
	case expr_func_call:
	case expr_condition:
	case expr_identifier:
	case expr_assign:
	case expr_postfix_op:	
	case expr_str_literal:
		return false;
	case expr_unary_op:
		switch (expr->data.unary_op.operation)
		{
		case op_dereference:
		case op_prefix_inc:
		case op_prefix_dec:
		case op_postfix_inc:
		case op_postfix_dec:
			return false;
		}

		return sema_is_int_constant_expression(expr->data.unary_op.expression);
	case expr_binary_op:
		switch (expr->data.binary_op.operation)
		{
		case op_member_access:
		case op_ptr_member_access:
		case op_array_subscript:
			return false;
		}		
		return sema_is_int_constant_expression(expr->data.binary_op.lhs) && sema_is_int_constant_expression(expr->data.binary_op.rhs);
	case expr_int_literal:	
	case expr_sizeof:
	case expr_null:
		break;
	}
	return true;
}

int_val_t sema_eval_constant_expr(ast_expression_t* expr)
{
	switch (expr->kind)
	{
	case expr_unary_op:
	{
		int_val_t v = sema_eval_constant_expr(expr->data.unary_op.expression);
		return int_val_unary_op(&v, expr->data.unary_op.operation);
	}
	case expr_binary_op:
	{
		int_val_t lhs = sema_eval_constant_expr(expr->data.binary_op.lhs);
		int_val_t rhs = sema_eval_constant_expr(expr->data.binary_op.rhs);
		return int_val_binary_op(&lhs, &rhs, expr->data.binary_op.operation);
	}

	case expr_int_literal:
		return expr->data.int_literal.val;
	case expr_sizeof:
		if (!process_sizeof_expr(expr))
			return int_val_zero(); //this is not ideal
		return expr->data.int_literal.val;
	case expr_null:
		break;
	}

	return int_val_zero();
}