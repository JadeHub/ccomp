#include "ast.h"

#include <stdlib.h>

void ast_destroy_statement(ast_statement_t* smnt);

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
	case expr_postfix_op:
		ast_destroy_expression(expr->data.unary_op.expression);
		break;
	case expr_condition:
		ast_destroy_expression(expr->data.condition.cond);
		ast_destroy_expression(expr->data.condition.true_branch);
		ast_destroy_expression(expr->data.condition.false_branch);
	}
	free(expr);
}

void ast_destroy_var_decl(ast_var_decl_t* v)
{
	if (!v) return;
	ast_destroy_expression(v->expr);
	free(v);
}

void ast_destroy_block_item(ast_block_item_t* b)
{
	if (!b) return;
	switch (b->kind)
	{
	case blk_smnt:
		ast_destroy_statement(b->smnt);
		break;
	case blk_var_def:
		ast_destroy_var_decl(b->var_decl);
	};
	free(b);

}

void ast_destroy_statement(ast_statement_t* smnt)
{
	if (!smnt) return;

	ast_block_item_t* block = NULL;
	switch (smnt->kind)
	{
		case smnt_return:
		case smnt_expr:
			ast_destroy_expression(smnt->data.expr);
			break;
		case smnt_if:
			ast_destroy_expression(smnt->data.if_smnt.condition);
			ast_destroy_statement(smnt->data.if_smnt.true_branch);
			ast_destroy_statement(smnt->data.if_smnt.false_branch);
			break;
		case smnt_compound:
			block = smnt->data.compound.blocks;
			while (block)
			{
				ast_block_item_t* next = block->next;
				ast_destroy_block_item(block);
				block = next;
			}
			break;
		case smnt_break:
		case smnt_continue:
			break;
		case smnt_while:
		case smnt_do:
			ast_destroy_expression(smnt->data.while_smnt.condition);
			ast_destroy_statement(smnt->data.while_smnt.statement);
			break;
		case smnt_for:
		case smnt_for_decl:
			ast_destroy_var_decl(smnt->data.for_smnt.init_decl);
			ast_destroy_expression(smnt->data.for_smnt.init);
			ast_destroy_expression(smnt->data.for_smnt.condition);
			ast_destroy_expression(smnt->data.for_smnt.post);
			ast_destroy_statement(smnt->data.for_smnt.statement);
			break;
	}
	free(smnt);
}

void ast_destroy_function_decl(ast_function_decl_t* fn)
{
	if (!fn) return;
	ast_block_item_t* smnt = fn->blocks;
	while (smnt)
	{
		ast_block_item_t* next = smnt->next;
		ast_destroy_block_item(smnt);
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