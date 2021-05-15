#include "sema_internal.h"

#include "std_types.h"

#include <assert.h>
#include <string.h>

static bool _is_const_int_type(ast_type_spec_t* spec)
{
	return ast_type_is_int(spec) || ast_type_is_ptr(spec);
}

bool sema_is_const_int_expr(ast_expression_t* expr)
{
	switch (expr->kind)
	{
	case expr_func_call:
	case expr_condition:
	case expr_identifier:
	case expr_str_literal:
	case expr_null:
	case expr_compound_init:
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

static ast_expression_t* _conv_expr_to_int_literal(ast_expression_t* expr, int_val_t val)
{
	ast_destroy_expression_data(expr);
	memset(expr, 0, sizeof(ast_expression_t));
	expr->kind = expr_int_literal;
	expr->data.int_literal.val = val;
	return expr;
}

ast_expression_t* _sema_eval_constant_expr(ast_expression_t* expr)
{
	switch (expr->kind)
	{
	case expr_unary_op:
	{
		ast_expression_t* inner = _sema_eval_constant_expr(expr->data.unary_op.expression);
		if (!inner)
			return NULL;

		int_val_t val = int_val_unary_op(&inner->data.int_literal.val, expr->data.unary_op.operation);

		expr = _conv_expr_to_int_literal(expr, val);
		expr->sema.result.type = inner->sema.result.type; //todo
		break;
	}
	case expr_binary_op:
	{
		ast_expression_t * lhs = _sema_eval_constant_expr(expr->data.binary_op.lhs);
		ast_expression_t* rhs = _sema_eval_constant_expr(expr->data.binary_op.rhs);
		if (!lhs || !rhs)
			return NULL;

		int_val_t val = int_val_binary_op(&lhs->data.int_literal.val, &rhs->data.int_literal.val, expr->data.binary_op.operation);

		expr = _conv_expr_to_int_literal(expr, val);
		expr->sema.result.type = lhs->sema.result.type; //todo
		break;
	}
	case expr_int_literal:
		if(sema_process_int_literal(expr).failure)
			return NULL;
		break;
	case expr_sizeof:
		if (!process_sizeof_expr(expr))
			return NULL;
		break;
	}

	return expr;
}

ast_expression_t* sema_fold_const_int_expr(ast_expression_t* expr)
{
	assert(sema_is_const_int_expr(expr));
	return _sema_eval_constant_expr(expr);
	
}