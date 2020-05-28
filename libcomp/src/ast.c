#include "ast.h"

void ast_destroy_expression(ast_expression_t* expr)
{
	if (!expr) return;
	switch (expr->kind)
	{
	case expr_assign:
		ast_destroy_expression(expr->data.assignment.expr);
		break;
	case expr_binary_op:
		ast_destroy_expression(expr->data.binary_op.lhs);
		ast_destroy_expression(expr->data.binary_op.rhs);
		break;
	case expr_int_literal:
		break;
	case expr_unary_op:
		ast_destroy_expression(expr->data.unary_op.expression);
		break;
	case expr_var_ref:
		break;
	}
	free(expr);
}

void ast_destroy_statement(ast_statement_t* smnt)
{
	if (!smnt) return;
	ast_destroy_expression(smnt->expr);
	free(smnt);
}

void ast_destroy_function_decl(ast_function_decl_t* fn)
{
	if (!fn) return;
	ast_statement_t* smnt = fn->statements;
	while (smnt)
	{
		ast_statement_t* next = smnt->next;
		ast_destroy_statement(smnt);
		smnt = next;
	}
	free(fn);
}

void ast_destory_translation_unit(ast_trans_unit_t* tl)
{
	if (!tl) return;
	ast_destroy_function_decl(tl->function);
	free(tl);
}