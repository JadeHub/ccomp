#include "ast.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

void ast_destroy_statement(ast_statement_t* smnt);
void ast_destroy_type_ref(ast_type_ref_t* type_ref);

//destroy any data held by the expression. used to 'reset' an expression before reusing
void ast_destroy_expression_data(ast_expression_t* expr)
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
	//don't free the expression
	expr->kind = expr_null;
}

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

void ast_destroy_struct_member(ast_struct_member_t* member)
{
	ast_destroy_type_ref(member->type_ref);
	free(member);
}

void ast_destroy_enum_member(ast_enum_member_t* member)
{
	ast_destroy_expression(member->const_value);
	free(member);
}

void ast_destroy_user_type(ast_user_type_spec_t* user_type)
{
	if (user_type->kind == user_type_enum)
	{
		ast_enum_member_t* member = user_type->data.enum_members;
		while (member)
		{
			ast_enum_member_t* next = member->next;
			ast_destroy_enum_member(member);
			member = next;
		}
	}
	else
	{
		ast_struct_member_t* member = user_type->data.struct_members;
		while (member)
		{
			ast_struct_member_t* next = member->next;
			ast_destroy_struct_member(member);
			member = next;
		}
	}
	free(user_type);
}

void ast_destroy_type_spec(ast_type_spec_t* type)
{
	type;
	/*if (!type) return;

	if (type->kind == type_ptr)
	{
		ast_destroy_type_spec(type->ptr_type);
		free(type);
	}
	else if (type->kind == type_user)
	{
		ast_destroy_user_type(type->user_type_spec);
	}*/
}

void ast_destroy_type_ref(ast_type_ref_t* type_ref)
{
	if (!type_ref) return;
	//we dont destroy the type here, that happens when we destroy the declaration
	free(type_ref);
}

void ast_destroy_declaration(ast_declaration_t* decl)
{
	if (!decl) return;

	switch (decl->kind)
	{
	case decl_var:
		ast_destroy_type_ref(decl->data.var.type_ref);
		ast_destroy_expression(decl->data.var.expr);
		break;
	case decl_func:
		//ast_destroy_expression(decl->data.func.params);
		break;
	case decl_type:
		ast_destroy_type_spec(decl->data.type);
		break;
	case decl_const:
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
		ast_destroy_statement(b->data.smnt);
		break;
	case blk_decl:
		ast_destroy_declaration(b->data.decl);
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
	
	ast_declaration_t* decl = tl->decls;

	while (decl)
	{
		ast_declaration_t* next = decl->next;
		ast_destroy_declaration(decl);
		decl = next;
	}

	free(tl);
}

const char* ast_type_ref_name(ast_type_ref_t* ref)
{ 
	return ast_type_name(ref->spec); 
}

const char* ast_type_name(ast_type_spec_t* type)
{
	switch (type->kind)
	{
	case type_void:
		return "void";
	case type_int8:
		return "int8";
	case type_uint8:
		return "uint8";
	case type_int16:
		return "int16";
	case type_uint16:
		return "uint16";
	case type_int32:
		return "int32";
	case type_uint32:
		return "uint32";
	case type_user:
		return type->data.user_type_spec->name;
	case type_ptr:
		return "pointer"; //todo
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
		return ast_type_name(decl->data.type);
	case decl_const:
		return decl->data.const_val.name;
	}
	return NULL;
}

ast_struct_member_t* ast_find_struct_member(ast_user_type_spec_t* user_type_spec, const char* name)
{
	assert(user_type_spec->kind != user_type_enum);
	ast_struct_member_t* member = user_type_spec->data.struct_members;
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
	return member->type_ref->spec->size;
}

uint32_t ast_struct_size(ast_user_type_spec_t* spec)
{
	assert(spec->kind != user_type_enum);
	uint32_t total = 0;
	uint32_t max_member = 0;

	ast_struct_member_t* member = spec->data.struct_members;
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

ast_type_spec_t* ast_make_ptr_type(ast_type_spec_t* ptr_type)
{
	ast_type_spec_t* result = (ast_type_spec_t*)malloc(sizeof(ast_type_spec_t));
	memset(result, 0, sizeof(ast_type_spec_t));
	result->kind = type_ptr;
	result->size = 4;
	result->data.ptr_type = ptr_type;
	return result;
}

bool ast_type_is_signed_int(ast_type_spec_t* type)
{
	switch (type->kind)
	{
	case type_int8:
	case type_int16:
	case type_int32:
	case type_int64:
		return true;
	}
	return false;
}

bool ast_type_is_int(ast_type_spec_t* spec)
{
	switch (spec->kind)
	{
	case type_int8:
	case type_uint8:
	case type_int16:
	case type_uint16:
	case type_int32:
	case type_uint32:
	case type_int64:
	case type_uint64:
		return true;
	}

	return spec->kind == type_user && spec->data.user_type_spec->kind == user_type_enum;
}
