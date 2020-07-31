#include "ast.h"

#include <stdlib.h>
#include <string.h>

void ast_destroy_statement(ast_statement_t* smnt);

void ast_destroy_expression(ast_expression_t* expr)
{
	if (!expr) return;

	switch (expr->kind)
	{
	case expr_postfix_op:
	case expr_unary_op:
		ast_destroy_expression(expr->data.unary_op.expression);
		break;
	case expr_binary_op:
		ast_destroy_expression(expr->data.binary_op.lhs);
		ast_destroy_expression(expr->data.binary_op.rhs);
		break;
	case expr_int_literal:
		break;
	case expr_assign:
		ast_destroy_expression(expr->data.assignment.expr);
		break;
	case expr_identifier:
		break;	
	case expr_condition:
		ast_destroy_expression(expr->data.condition.cond);
		ast_destroy_expression(expr->data.condition.true_branch);
		ast_destroy_expression(expr->data.condition.false_branch);
		break;
	case expr_func_call:
		ast_destroy_expression(expr->data.func_call.params);
		break;
	}

	ast_destroy_expression(expr->next);
	free(expr);
}

void ast_destroy_type_spec(ast_type_spec_t* type)
{
//	if (!type)
		return;

	/*if (type->kind == type_struct)
	{
		ast_struct_member_t* member = type->struct_spec->members;

		while (member)
		{
			ast_struct_member_t* next = member->next;
			ast_destroy_type_spec(member->type);
			free(member);
			member = next;
		}
		free(type->struct_spec);
	}*/
	free(type);
}

void ast_destroy_declaration(ast_declaration_t* decl)
{
	if (!decl) return;

	switch (decl->kind)
	{
	case decl_var:
		ast_destroy_expression(decl->data.var.expr);
		break;
	case decl_func:
		//ast_destroy_expression(decl->data.func.params);
		break;
	}
	free(decl);
}

void ast_destroy_block_item(ast_block_item_t* b)
{
	if (!b) return;
	switch (b->kind)
	{
	case blk_smnt:
		ast_destroy_statement(b->smnt);
		break;
	case blk_decl:
		ast_destroy_declaration(b->decl);
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
			ast_destroy_declaration(smnt->data.for_smnt.init_decl);
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
	/*ast_block_item_t* smnt = fn->blocks;
	while (smnt)
	{
		ast_block_item_t* next = smnt->next;
		ast_destroy_block_item(smnt);
		smnt = next;
	}*/

	/*ast_function_param_t* param = fn->params;

	while (param)
	{
		ast_function_param_t* next = param->next;
		free(param);
		param = next;
	}*/

	free(fn);
}
void ast_destory_translation_unit(ast_trans_unit_t* tl)
{
	if (!tl) return;
	//ast_destroy_function_decl(tl->functions);
	free(tl);
}

/*bool ast_visit_statement_helper(ast_statement_t* smnt, ast_visitor_cb_t* cb)
{
	switch (smnt->kind)
	{
	case smnt_return:
		if (cb->on_return_smnt && !cb->on_return_smnt(smnt, cb))
			return false;
		break;
	case smnt_expr:
		if (cb->on_expr_smnt && !cb->on_expr_smnt(smnt, cb))
			return false;
		break;
	case smnt_if:
		if (cb->on_if_smnt && !cb->on_if_smnt(smnt, cb))
			return false;
		break;

	};
}*/


/*bool ast_visit_statement_expressions(ast_statement_t* smnt, ast_expr_visitor_cb cb)
{
	switch (smnt->kind)
	{
	case smnt_expr:
		return cb(smnt->data.expr);
		break;
	case smnt_do:
	case smnt_while:
		if (!cb(smnt->data.while_smnt.condition))
			return false;
		if (!cb(ast_visit_statement_expressions(smnt->data.while_smnt.statement, cb)))
			return false;
		break;
	case smnt_for:

	}
	return true;
}
*/
bool ast_visit_block_items(ast_block_item_t* blocks, ast_block_item_visitor_cb cb)
{
	ast_block_item_t* block = blocks;
	ast_block_item_t* next;

	while (block)
	{
		next = block->next;
		if (!cb(block))
			return false;
		block = next;
	}
	return true;
}

const char* ast_type_name(ast_type_spec_t* type)
{
	switch (type->kind)
	{
	case type_void:
		return "void";
	case type_int:
		return "int";
	case type_user:
		return type->struct_spec->name;
	}
	return "unknown type";
}

const char* ast_declaration_name(ast_declaration_t* decl)
{
	switch (decl->kind)
	{
	case decl_var:
		return decl->data.var.name;
	case decl_func:
		return decl->data.func.name;
	case decl_type:
		return ast_type_name(&decl->data.type);
	}
	return NULL;
}

ast_struct_member_t* ast_find_struct_member(ast_struct_spec_t* struct_spec, const char* name)
{
	ast_struct_member_t* member = struct_spec->members;
	while (member)
	{
		if (strcmp(member->name, name) == 0)
		{
			//return member->offset;
			return member;
		}
		member = member->next;
	}
	return NULL;
}

uint32_t ast_struct_member_size(ast_struct_member_t* member)
{
	if (member->bit_size > 0)
		return (member->bit_size / 8) + (member->bit_size % 8 ? 1 : 0);
	return member->type->size;
}

uint32_t ast_struct_size(ast_struct_spec_t* spec)
{
	uint32_t total = 0;
	uint32_t max_member = 0;

	ast_struct_member_t* member = spec->members;
	while (member)
	{
		uint32_t size = ast_struct_member_size(member);
		total += size;
		if (size > max_member)
			max_member = size;
		member = member->next;
	}
	return spec->kind == user_type_union ? max_member : total;
}