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
	case expr_unary_op:
		ast_destroy_expression(expr->data.unary_op.expression);
		break;
	case expr_binary_op:
		ast_destroy_expression(expr->data.binary_op.lhs);
		ast_destroy_expression(expr->data.binary_op.rhs);
		break;
	case expr_int_literal:
		break;
	case expr_identifier:
		break;
	case expr_condition:
		ast_destroy_expression(expr->data.condition.cond);
		ast_destroy_expression(expr->data.condition.true_branch);
		ast_destroy_expression(expr->data.condition.false_branch);
		break;
	case expr_func_call:
	{
		ast_destroy_expression(expr->data.func_call.target);
		ast_func_call_param_t* param = expr->data.func_call.first_param;
		while (param)
		{
			ast_func_call_param_t* next = param->next;
			ast_destroy_expression(param->expr);
			free(param);
			param = next;
		}
		break;
	}
	case expr_sizeof:
		if (expr->data.sizeof_call.kind == sizeof_expr)
			ast_destroy_expression(expr->data.sizeof_call.data.expr);
		break;
	case expr_cast:
		ast_destroy_expression(expr->data.cast.expr);
		break;
	}

	//don't free the expression
	expr->kind = expr_null;
}

void ast_destroy_expression(ast_expression_t* expr)
{
	if (!expr) return;

	switch (expr->kind)
	{
	case expr_unary_op:
		ast_destroy_expression(expr->data.unary_op.expression);
		break;
	case expr_binary_op:
		ast_destroy_expression(expr->data.binary_op.lhs);
		ast_destroy_expression(expr->data.binary_op.rhs);
		break;
	case expr_int_literal:
		break;
	case expr_identifier:
		break;	
	case expr_condition:
		ast_destroy_expression(expr->data.condition.cond);
		ast_destroy_expression(expr->data.condition.true_branch);
		ast_destroy_expression(expr->data.condition.false_branch);
		break;
	case expr_func_call:
	{
		ast_func_call_param_t* param = expr->data.func_call.first_param;
		while (param)
		{
			ast_func_call_param_t* next = param->next;
			ast_destroy_expression(param->expr);
			free(param);
			param = next;
		}
	}
		break;
	}
		
	free(expr);
}

void ast_destroy_struct_member(ast_struct_member_t* member)
{
	ast_destroy_declaration(member->decl);
	free(member);
}

void ast_destroy_enum_member(ast_enum_member_t* member)
{
	ast_destroy_expression(member->value);
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

	ast_destroy_type_ref(decl->type_ref);
	switch (decl->kind)
	{
	case decl_var:
		ast_destroy_expression(decl->data.var.init_expr);
		break;
	case decl_func:
		//ast_destroy_expression(decl->data.func.params);
		break;
	}
	free(decl);
}

void ast_destroy_expression_list(ast_expression_list_t* list)
{
	while (list)
	{
		ast_expression_list_t* next = list->next;
		ast_destroy_expression(list->expr);
		free(list);
		list = next;
	}
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
		ast_destroy_decl_list(b->data.decls);
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
			ast_destroy_decl_list(smnt->data.for_smnt.decls);
			ast_destroy_expression(smnt->data.for_smnt.init);
			ast_destroy_expression(smnt->data.for_smnt.condition);
			ast_destroy_expression(smnt->data.for_smnt.post);
			ast_destroy_statement(smnt->data.for_smnt.statement);
			break;
	}
	free(smnt);
}

void ast_destroy_function_decl(ast_function_decl_data_t* fn)
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

void ast_destroy_decl_list(ast_decl_list_t decl_list)
{
	ast_declaration_t* decl = decl_list.first;

	while (decl)
	{
		ast_declaration_t* next = decl->next;
		ast_destroy_declaration(decl);
		decl = next;
	}
}

void ast_destory_translation_unit(ast_trans_unit_t* tl)
{
	if (!tl) return;

	ast_destroy_decl_list(tl->decls);

	free(tl);
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
	case type_array:
		return "array"; //todo
	case type_ptr:
		return "pointer"; //todo
	}
	return "unknown type";
}

void ast_type_spec_desc(str_buff_t* sb, ast_type_spec_t* spec)
{
	switch (spec->kind)
	{
	case type_void:
		sb_append(sb, "void");
		break;
	case type_int8:
		sb_append(sb, "int8");
		break;
	case type_uint8:
		sb_append(sb, "uint8");
		break;
	case type_int16:
		sb_append(sb, "int16");
		break;
	case type_uint16:
		sb_append(sb, "uint16");
		break;
	case type_int32:
		sb_append(sb, "int32");
		break;
	case type_uint32:
		sb_append(sb, "uint32");
		break;
	case type_user:
		sb_append(sb, ast_user_type_kind_name(spec->data.user_type_spec->kind));
		sb_append(sb, " ");
		if(strlen(spec->data.user_type_spec->name))
			sb_append(sb, spec->data.user_type_spec->name);
		else
			sb_append(sb, "<anonymous>");
		break;
	case type_ptr:
		ast_type_spec_desc(sb, spec->data.ptr_type);
		sb_append_ch(sb, '*');
		break;
	}
}

void ast_type_ref_desc(str_buff_t* sb, ast_type_ref_t* type_ref)
{
	if (type_ref->flags & TF_SC_TYPEDEF)
		sb_append(sb, "typedef ");
	if (type_ref->flags & TF_SC_EXTERN)
		sb_append(sb, "extern ");
	if (type_ref->flags & TF_SC_STATIC)
		sb_append(sb, "static ");
	if (type_ref->flags & TF_SC_INLINE)
		sb_append(sb, "inline ");
	if (type_ref->flags & TF_QUAL_CONST)
		sb_append(sb, "const ");

//	ast_type_spec_desc(sb, type_ref->spec);
}

void ast_decl_type_describe(str_buff_t* sb, ast_declaration_t* decl)
{
	if (ast_type_is_array(decl->type_ref->spec))
	{
		ast_type_ref_desc(sb, decl->type_ref);
		ast_type_spec_desc(sb, decl->type_ref->spec->data.array_spec->element_type);

		sb_append_ch(sb, '[');
		sb_append(sb, "to do");
		//sb_append_int(sb, decl->array_spec->sema.total_items, 10);
		sb_append_ch(sb, ']');
	}
	else if (decl->kind == decl_var || decl->kind == decl_type)
	{
		ast_type_ref_desc(sb, decl->type_ref);
		ast_type_spec_desc(sb, decl->type_ref->spec);
	}
}

const char* ast_declaration_name(ast_declaration_t* decl)
{
	return decl->name;
}

ast_struct_member_t* ast_find_struct_member(ast_user_type_spec_t* user_type_spec, const char* name)
{
	assert(user_type_spec->kind != user_type_enum);
	ast_struct_member_t* member = user_type_spec->data.struct_members;
	while (member)
	{
		if (strcmp(member->decl->name, name) == 0)
		{
			return member;
		}
		member = member->next;
	}
	return NULL;
}

ast_type_spec_t* ast_make_array_type(ast_type_spec_t* element_type, ast_expression_t* size_expr)
{
	ast_type_spec_t* result = (ast_type_spec_t*)malloc(sizeof(ast_type_spec_t));
	memset(result, 0, sizeof(ast_type_spec_t));
	result->kind = type_array;
	result->size = 0;
	result->data.array_spec = (ast_array_spec_t*)malloc(sizeof(ast_array_spec_t));
	memset(result->data.array_spec, 0, sizeof(ast_array_spec_t));
	result->data.array_spec->element_type = element_type;
	result->data.array_spec->size_expr = size_expr;
	return result;
}

ast_type_spec_t* ast_make_ptr_type(ast_type_spec_t* type)
{
	ast_type_spec_t* result = (ast_type_spec_t*)malloc(sizeof(ast_type_spec_t));
	memset(result, 0, sizeof(ast_type_spec_t));
	result->kind = type_ptr;
	result->size = 4;
	result->data.ptr_type = type;
	return result;
}

ast_type_spec_t* ast_make_func_sig_type(ast_type_spec_t* ret_type, ast_func_params_t* params)
{
	ast_type_spec_t* result = (ast_type_spec_t*)malloc(sizeof(ast_type_spec_t));
	memset(result, 0, sizeof(ast_type_spec_t));
	result->kind = type_func_sig;
	result->size = 4;

	result->data.func_sig_spec = (ast_func_sig_type_spec_t*)malloc(sizeof(ast_func_sig_type_spec_t));
	memset(result->data.func_sig_spec, 0, sizeof(result->data.func_sig_spec));
	result->data.func_sig_spec->ret_type = ret_type;
	result->data.func_sig_spec->params = params;
	return result;
}

ast_type_spec_t* ast_func_decl_return_type(ast_declaration_t* fn)
{
	assert(fn->kind == decl_func);
	return fn->type_ref->spec->data.func_sig_spec->ret_type;
}

ast_func_params_t* ast_func_decl_params(ast_declaration_t* fn)
{
	assert(fn->kind == decl_func);
	return fn->type_ref->spec->data.func_sig_spec->params;
}

bool ast_expr_is_int_literal(ast_expression_t* expr, int64_t val)
{
	return expr->kind == expr_int_literal && expr->data.int_literal.val.v.int64 == val;
}

bool ast_type_is_fn_ptr(ast_type_spec_t* type)
{
	return type->kind == type_ptr && type->data.ptr_type->kind == type_func_sig;
}

bool ast_type_is_array(ast_type_spec_t* type)
{
	return type->kind == type_array;
}

bool ast_type_is_alias(ast_type_spec_t* type)
{
	return type->kind == type_alias;
}

bool ast_type_is_ptr(ast_type_spec_t* type)
{
	return type->kind == type_ptr;
}

ast_type_spec_t* ast_make_func_ptr_type(ast_type_spec_t* ret_type, ast_func_params_t* params)
{
	return ast_make_ptr_type(ast_make_func_sig_type(ret_type, params));
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

	return ast_type_is_enum(spec);
}

bool ast_type_is_enum(ast_type_spec_t* type)
{
	return type->kind == type_user && type->data.user_type_spec->kind == user_type_enum;
}

bool ast_type_is_struct_union(ast_type_spec_t* type)
{
	return type->kind == type_user && type->data.user_type_spec->kind != user_type_enum;
}

bool ast_type_is_void_ptr(ast_type_spec_t* spec)
{
	return spec->kind == type_ptr &&
		spec->data.ptr_type->kind == type_void;
}

bool ast_is_bit_field_member(ast_struct_member_t* member)
{
	return member->decl->data.var.bit_sz != NULL;
}

bool ast_is_assignment_op(op_kind op)
{
	switch (op)
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
		return true;
	}
	return false;
}

const char* ast_op_name(op_kind k)
{
	switch (k)
	{
	case op_assign:
		return "[=] assign";
	case op_mul_assign:
		return "[*=] mul assign";
	case op_div_assign:
		return "[/=] div assign";
	case op_mod_assign:
		return "[%=] mod assign";
	case op_add_assign:
		return "[+=] add assign";
	case op_sub_assign:
		return "[-=] sub assign";
	case op_left_shift_assign:
		return "[<<=] left shift assign";
	case op_right_shift_assign:
		return "[>>=] right shift assign";
	case op_and_assign:
		return "[&=] and assign";
	case op_xor_assign:
		return "[^=] xor assign";
	case op_or_assign:
		return "[|=] or assign";
	case op_sub:
		return "[-] Subtraction";
	case op_negate:
		return "[-] Negation";
	case op_compliment:
		return "[~] Compliment";
	case op_not:
		return "[!] Not";
	case op_add:
		return "[+] Addition";
	case op_mul:
		return "[*] Multiplication";
	case op_div:
		return "[/] Division";
	case op_and:
		return "[&&] And";
	case op_or:
		return "[||] Or";
	case op_eq:
		return "[==] Equal";
	case op_neq:
		return "[!=] Not equal";
	case op_lessthan:
		return "[<] Less than";
	case op_lessthanequal:
		return "[<=] Less than or equal";
	case op_greaterthan:
		return "[>] Greater than";
	case op_greaterthanequal:
		return "[>] Greater than or equal";
	case op_shiftleft:
		return "[<<] Shift left";
	case op_shiftright:
		return "[>>] Shift right";
	case op_bitwise_and:
		return "[&] Bitwise and";
	case op_bitwise_or:
		return "[|] Bitwise or";
	case op_bitwise_xor:
		return "[^] Bitwise xor";
	case op_mod:
		return "[%] Modulo";
	case op_prefix_inc:
		return "[++] Prefix inc";
	case op_prefix_dec:
		return "[-] Prefix dec";
	case op_postfix_inc:
		return "[++] Postfix inc";
	case op_postfix_dec:
		return "[--] Postfix inc";
	case op_member_access:
		return "[.] Member access";
	case op_ptr_member_access:
		return "[->] Pre member access";
	case op_address_of:
		return "[&] Address of";
	case op_dereference:
		return "[*] Defererence";
	}
	return "ERROR";
}