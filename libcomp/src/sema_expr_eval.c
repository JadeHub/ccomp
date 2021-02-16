#include "sema_internal.h"

#include <assert.h>
#include <string.h>

bool sema_is_const_int_expr(ast_expression_t* expr)
{
	switch (expr->kind)
	{
	case expr_func_call:
	case expr_condition:
	case expr_identifier:
	case expr_str_literal:
	case expr_null:
	case expr_cast:
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

		return sema_is_const_int_expr(expr->data.unary_op.expression);
	case expr_binary_op:
		switch (expr->data.binary_op.operation)
		{
		case op_member_access:
		case op_ptr_member_access:
		case op_array_subscript:
		case op_assign: //...
			return false;
		}		
		return sema_is_const_int_expr(expr->data.binary_op.lhs) && sema_is_const_int_expr(expr->data.binary_op.rhs);
	case expr_int_literal:	
	case expr_sizeof:
	
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
		sema_process_int_literal(expr);
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

int_val_t sema_fold_const_int_expr(ast_expression_t* expr)
{
	if (expr->kind == expr_int_literal)
		return expr->data.int_literal.val;

	assert(sema_is_const_int_expr(expr));
	int_val_t result = sema_eval_constant_expr(expr);
	ast_destroy_expression_data(expr);
	memset(expr, 0, sizeof(ast_expression_t));
	expr->kind = expr_int_literal;
	expr->data.int_literal.val = result;
	return result;
}