#include "sema.h"
#include "diag.h"
#include "var_set.h"
#include "id_map.h"

#include "std_types.h"

#include <libj/include/hash_table.h>

#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

static identfier_map_t* _id_map;

typedef struct
{
	ast_function_decl_t* decl;

	//set of goto statements found in the function
	hash_table_t* goto_smnts;

	//set of strings used as labels
	hash_table_t* labels;
}func_context_t;

static func_context_t _cur_func_ctx;

typedef enum
{
	proc_decl_new_def,
	proc_decl_ignore,
	proc_decl_error
}proc_decl_result;

proc_decl_result process_function_decl(ast_declaration_t* decl);
bool process_expression(ast_expression_t* expr);
bool process_statement(ast_statement_t* smnt);
static bool _resolve_type_ref(ast_type_ref_t* ref);

static void _report_err(token_t* tok, int err, const char* format, ...)
{
	char buff[512];

	va_list args;
	va_start(args, format);
	vsnprintf(buff, 512, format, args);
	va_end(args);

	diag_err(tok, err, buff);
}

static ast_type_spec_t* _resolve_type(ast_type_spec_t* typeref);

static void _resolve_struct_member_types(ast_user_type_spec_t* user_type_spec)
{
	assert(user_type_spec->kind != user_type_enum);
	ast_struct_member_t* member = user_type_spec->struct_members;
	while (member)
	{
		_resolve_type_ref(member->type_ref);
		member = member->next;
	}
}

static void _register_enum_constants(ast_user_type_spec_t* user_type_spec)
{
	assert(user_type_spec->kind == user_type_enum);
	ast_enum_member_t* member = user_type_spec->enum_members;
	while (member)
	{
		ast_declaration_t* decl = (ast_declaration_t*)malloc(sizeof(ast_declaration_t));
		memset(decl, 0, sizeof(ast_declaration_t));
		decl->tokens = member->tokens;
		decl->kind = decl_const;
		strcpy(decl->data.const_val.name, member->name);
		decl->data.const_val.type = uint32_type_spec;
		decl->data.const_val.expr = member->const_value;
		decl->data.const_val.expr->data.int_literal.type = uint32_type_spec;

		idm_add_id(_id_map, decl);

		member = member->next;
	}
}

static bool _user_type_is_definition(ast_user_type_spec_t* type)
{
	if (type->kind == user_type_enum)
		return type->enum_members != NULL;
	return type->struct_members != NULL;
}

static uint32_t _calc_user_type_size(ast_type_spec_t* typeref)
{
	if (typeref->user_type_spec->kind == user_type_enum)
		return 4;

	uint32_t result = 0;
	ast_struct_member_t* member = typeref->user_type_spec->struct_members;
	while (member)
	{
		result += member->type_ref->spec->size;
		member = member->next;
	}
	return result;
}

static void _add_user_type(ast_type_spec_t* typeref)
{
	if (typeref->user_type_spec->kind == user_type_enum)
		_register_enum_constants(typeref->user_type_spec);
	else
		_resolve_struct_member_types(typeref->user_type_spec);
	typeref->size = _calc_user_type_size(typeref);
	idm_add_tag(_id_map, typeref);
}

static bool _resolve_type_ref(ast_type_ref_t* ref)
{
	assert(ref->spec);
	ast_type_spec_t* spec = _resolve_type(ref->spec);
	if (spec)
	{
		ref->spec = spec;
		return true;
	}
	return false;
}

static ast_type_spec_t* _resolve_type(ast_type_spec_t* typeref)
{
	if (typeref->kind == type_ptr)
	{
		typeref->ptr_type = _resolve_type(typeref->ptr_type);
		return typeref;
	}

	if (typeref->kind != type_user)
		return typeref;

	/*
	structs, unions & enums

	if typeref is a definition
		Look for a type with the same name in the current lexical block
			If declaration found: Update with definition
			If definition found: Error duplicate definition
			If not found: Add to id_map
	else if typeref is a declaration
		Look for a type with the same name in any lexical block
		If found: return
		If not found: Add to id_map
	*/
	token_t* loc = typeref->user_type_spec->tokens.start;
	if (_user_type_is_definition(typeref->user_type_spec))
	{
		ast_type_spec_t* exist = idm_find_block_tag(_id_map, typeref->user_type_spec->name);
		if (typeref == exist)
			return exist;

		if (exist)
		{
			if (exist->user_type_spec->kind != typeref->user_type_spec->kind)
			{
				_report_err(loc, ERR_DUP_TYPE_DEF,
					"redefinition of %s '%s' changes type to %s",
					user_type_kind_name(exist->user_type_spec->kind),
					typeref->user_type_spec->name,
					user_type_kind_name(typeref->user_type_spec->kind));
				return NULL;
			}

			if (_user_type_is_definition(exist->user_type_spec))
			{
				//multiple definitions
				_report_err(loc, ERR_DUP_TYPE_DEF,
					"redefinition of %s '%s'",
					user_type_kind_name(typeref->user_type_spec->kind),
					typeref->user_type_spec->name);
				return NULL;
			}
			//update the existing declaration
			if (typeref->user_type_spec->kind == user_type_enum)
			{
				exist->user_type_spec->enum_members = typeref->user_type_spec->enum_members;
				typeref->user_type_spec->enum_members = NULL;
				_register_enum_constants(exist->user_type_spec);
			}
			else
			{
				//update definition
				exist->user_type_spec->struct_members = typeref->user_type_spec->struct_members;
				typeref->user_type_spec->struct_members = NULL;
				_resolve_struct_member_types(exist->user_type_spec);
			}
			exist->size = _calc_user_type_size(exist);
			ast_destroy_type_spec(typeref);
			return exist;
		}
		else
		{
			_add_user_type(typeref);
		}
	}
	else
	{
		//declaration
		ast_type_spec_t* exist = idm_find_tag(_id_map, typeref->user_type_spec->name);
		if (exist)
		{
			if (exist->user_type_spec->kind != typeref->user_type_spec->kind)
			{
				_report_err(loc, ERR_DUP_TYPE_DEF,
					"redefinition of %s '%s' changes type to %s",
					user_type_kind_name(exist->user_type_spec->kind),
					typeref->user_type_spec->name,
					user_type_kind_name(typeref->user_type_spec->kind));
				return NULL;
			}
			ast_destroy_type_spec(typeref);
			return exist;
		}

		_add_user_type(typeref);
	}
	return typeref;
}

static bool _is_int_type(ast_type_spec_t* spec)
{
	switch (spec->kind)
	{
	case type_int8:
	case type_uint8:
	case type_int16:
	case type_uint16:
	case type_int32:
	case type_uint32:
		return true;
	}

	return spec->kind == type_user && spec->user_type_spec->kind == user_type_enum;
}

static bool _can_convert_type(ast_type_spec_t* target, ast_type_spec_t* type)
{
	if (target == type)
		return true;

	//integer promotion
	if (_is_int_type(target) && _is_int_type(type) && target->size >= type->size)
		return true;

	if (target->kind == type_ptr && type->kind == type_ptr)
		return _can_convert_type(target->ptr_type, type->ptr_type);

	return false;
}

bool process_variable_declaration(ast_declaration_t* decl)
{
	ast_var_decl_t* var = &decl->data.var;

	ast_declaration_t* existing = idm_find_block_decl(_id_map, var->name, decl_var);
	if (existing)
	{
		_report_err(decl->tokens.start, ERR_DUP_VAR, "var %s already declared at",
			ast_declaration_name(decl));
		return false;
	}

	if(!_resolve_type_ref(var->type_ref) || var->type_ref->spec->size == 0)
	{
		_report_err(decl->tokens.start, ERR_TYPE_INCOMPLETE, "var %s is of incomplete type",
			ast_declaration_name(decl));
		return false;
	}

	idm_add_id(_id_map, decl);
	_cur_func_ctx.decl->required_stack_size += var->type_ref->spec->size;

	return process_expression(decl->data.var.expr);
}

bool process_declaration(ast_declaration_t* decl)
{
	if (!decl)
		return true;
	switch (decl->kind)
	{
	case decl_var:
		return process_variable_declaration(decl);
	case decl_func:
		return process_function_decl(decl) != proc_decl_error;
	case decl_type:
		return _resolve_type(decl->data.type);		
	}
	return true;
}

bool process_variable_reference(ast_expression_t* expr)
{
	if (!expr)
		return true;

	ast_declaration_t* decl = idm_find_decl(_id_map, expr->data.var_reference.name, decl_var);
	if (decl)
	{
		if(decl->data.var.type_ref->spec->size == 0)
		{
			_report_err(decl->tokens.start, ERR_TYPE_INCOMPLETE, "var %s is of incomplete type",
				ast_declaration_name(decl));
			return false;
		}
		//decl->data.var.type = type;
		return true;
	}

	decl = idm_find_decl(_id_map, expr->data.var_reference.name, decl_const);
	if (decl)
	{
		expr->kind = expr_int_literal;
		expr->data.int_literal = decl->data.const_val.expr->data.int_literal;
		return process_expression(expr);
	}

	_report_err(expr->tokens.start, ERR_UNKNOWN_VAR, "unknown var %s",
		expr->data.var_reference.name);
	return false;
}

static ast_type_spec_t* _resolve_expr_type(ast_expression_t* expr)
{
	if (!expr)
		return NULL;

	switch (expr->kind)
	{
	case expr_postfix_op:
	case expr_unary_op:

		if (expr->data.unary_op.operation == op_address_of)
		{
			ast_type_spec_t* expr_type = _resolve_expr_type(expr->data.unary_op.expression);
			assert(expr_type);
			ast_type_spec_t* ptr_type = ast_make_ptr_type(expr_type);
			return _resolve_type(ptr_type);
		}
		else if (expr->data.unary_op.operation == op_dereference)
		{
			ast_type_spec_t* expr_type = _resolve_expr_type(expr->data.unary_op.expression);
			assert(expr_type->kind == type_ptr);
			return expr_type->ptr_type;
		}

		//todo handle signed/unisgned here, eg uint32 x = 5; int32 y = -x;
		return _resolve_expr_type(expr->data.unary_op.expression);
	case expr_binary_op:
	{
		if (expr->data.binary_op.operation == op_member_access)
		{
			// a.b (lhs.rhs)
			ast_type_spec_t* user_type_spec = _resolve_expr_type(expr->data.binary_op.lhs);
			assert(user_type_spec);
			ast_struct_member_t* member = ast_find_struct_member(user_type_spec->user_type_spec, expr->data.binary_op.rhs->data.var_reference.name);
			assert(member);
//			ast_type_spec_t* member_spec = ast_find_struct_member(user_type_spec->user_type_spec, expr->data.binary_op.rhs->data.var_reference.name)->type;
			//return _resolve_type(member_spec);
			return member->type_ref->spec;
		}

		if (expr->data.binary_op.operation == op_ptr_member_access)
		{
			// a->b (lhs.rhs)
			ast_type_spec_t* user_type_spec = _resolve_expr_type(expr->data.binary_op.lhs);
			assert(user_type_spec);
			ast_struct_member_t* member = ast_find_struct_member(user_type_spec->ptr_type->user_type_spec, expr->data.binary_op.rhs->data.var_reference.name);
			assert(member);
			return member->type_ref->spec;
		}
		
		ast_type_spec_t* l = _resolve_expr_type(expr->data.binary_op.lhs);
		ast_type_spec_t* r = _resolve_expr_type(expr->data.binary_op.rhs);
		if (l == r)
			return r;

		//can r be promoted to l?
		if (_can_convert_type(l, r))
			return l;

		//can l be promoted to r?
		if (_can_convert_type(r, l))
			return r;
		

		//if (!process_expression(expr->data.binary_op.lhs))
		//	return false;
		//return process_expression(expr->data.binary_op.rhs);
		break;
	}
	case expr_int_literal:
		assert(expr->data.int_literal.type);
		return expr->data.int_literal.type;
	case expr_assign:
		return _resolve_expr_type(expr->data.assignment.target);
	case expr_identifier:
	{
		ast_declaration_t* decl = idm_find_decl(_id_map, expr->data.var_reference.name, decl_var);
		if (decl)
			return decl->data.var.type_ref->spec;
		return NULL;
	}
	
	case expr_condition:
		//if (!process_expression(expr->data.condition.cond))
		//	return false;
		//if (!process_expression(expr->data.condition.true_branch))
		//	return false;
		//return process_expression(expr->data.condition.false_branch);
		return uint32_type_spec;
		break;
	case expr_func_call:
	{
		ast_declaration_t* decl = idm_find_decl(_id_map, expr->data.var_reference.name, decl_func);
		if (decl)
			return decl->data.func.return_type_ref->spec;
		return NULL;
	}
	case expr_sizeof:
		return uint32_type_spec;

	case expr_null:
		return void_type_spec;
		break;
	}
	return NULL;
}

bool process_func_call(ast_expression_t* expr)
{	
	if (!expr)
		return true;

	ast_declaration_t* decl = idm_find_decl(_id_map, expr->data.func_call.name, decl_func);
	if (!decl)
	{
		_report_err(expr->tokens.start,
			ERR_UNKNOWN_FUNC, 
			"function '%s' not defined", 
			expr->data.func_call.name);
		return false;
	}

	if (expr->data.func_call.param_count != decl->data.func.param_count)
	{
		_report_err(expr->tokens.start,
			ERR_INVALID_PARAMS,
			"incorrect number of params in call to function '%s'. Expected %d",
			expr->data.func_call.name, decl->data.func.param_count);
		return false;
	}

	/*
	Compare param types
	*/

	ast_expression_t* call_param = expr->data.func_call.params;
	ast_func_param_decl_t* func_param = decl->data.func.last_param;

	int p_count = 1;
	while (call_param && func_param)
	{
		if (!process_expression(call_param))
			return false;

		if (!_can_convert_type(func_param->decl->data.var.type_ref->spec, _resolve_expr_type(call_param)))
		{
			_report_err(expr->tokens.start,
				ERR_INVALID_PARAMS,
				"conflicting type in param %d of call to function '%s'. Expected '%s'",
				p_count, ast_declaration_name(decl), ast_type_name(func_param->decl->data.var.type_ref->spec));
			return false;
		}

		call_param = call_param->next;
		func_param = func_param->prev;
		p_count++;
	}
	expr->data.func_call.func_decl = decl;
	return true;
}

bool process_member_access_expression(ast_expression_t* expr)
{
	// a.b (lhs.rhs)
	if (!process_expression(expr->data.binary_op.lhs))
		return false;

	ast_type_spec_t* user_type_spec = _resolve_expr_type(expr->data.binary_op.lhs);

	if (expr->data.binary_op.operation == op_ptr_member_access)
	{
		if (user_type_spec->kind != type_ptr)
		{
			_report_err(expr->tokens.start,
				ERR_INCOMPATIBLE_TYPE,
				"'->' must be applied to a pointer type");
			return false;
		}
		user_type_spec = user_type_spec->ptr_type;
	}
	else
	{
		if (user_type_spec->kind == type_ptr)
		{
			_report_err(expr->tokens.start,
				ERR_INCOMPATIBLE_TYPE,
				"'.' cannot be applied to a pointer type");
			return false;
		}
	}
	
	if (user_type_spec->kind != type_user)
	{
		_report_err(expr->tokens.start,
			ERR_INCOMPATIBLE_TYPE,
			"'->' or '.' can only be applied to user defined types");
		return false;
	}

	if (expr->data.binary_op.rhs->kind != expr_identifier)
	{
		_report_err(expr->tokens.start,
			ERR_INCOMPATIBLE_TYPE,
			"operand of '->' or '.' must be an identifier");
		return false;
	}

	ast_struct_member_t* member = ast_find_struct_member(user_type_spec->user_type_spec, expr->data.binary_op.rhs->data.var_reference.name);
	if (member == NULL)
	{
		_report_err(expr->tokens.start,
			ERR_UNKNOWN_MEMBER_REF,
			"%s is not a member of %s",
			expr->data.binary_op.rhs->data.var_reference.name,
			ast_type_name(user_type_spec));
		return false;
	}
	
	return true;
}

bool process_assignment_expression(ast_expression_t* expr)
{
	if (!process_expression(expr->data.assignment.expr) || !process_expression(expr->data.assignment.target))
		return false;

	ast_type_spec_t* target_type = _resolve_expr_type(expr->data.assignment.target);
	ast_type_spec_t* expr_type = _resolve_expr_type(expr->data.assignment.expr);

	if (!target_type)
	{
		_report_err(expr->tokens.start,
			ERR_UNKNOWN_TYPE,
			"cannot determine assignment target type");
		return false;
	}

	if (!expr_type)
	{
		_report_err(expr->tokens.start,
			ERR_UNKNOWN_TYPE,
			"cannot determine assignment type");
		return false;
	}

	if (!_can_convert_type(target_type, expr_type))
	{
		_report_err(expr->tokens.start,
			ERR_INCOMPATIBLE_TYPE,
			"assignment to incompatible type. expected %s",
			ast_type_name(target_type));
		return false;
	}

	return process_expression(expr->data.assignment.target);
}

bool process_sizeof_expr(ast_expression_t* expr)
{
	ast_type_spec_t* type;
	
	if (expr->data.sizeof_call.kind == sizeof_type)
		type = _resolve_type(expr->data.sizeof_call.type);
	else
		type = _resolve_expr_type(expr->data.sizeof_call.expr);

	if (!type || type->size == 0)
	{
		_report_err(expr->tokens.start,
			ERR_UNKNOWN_TYPE,
			"could not determin type for sizeof call");
		return false;
	}

	//change expression to represent a constant int and re-process
	expr->kind = expr_int_literal;
	expr->data.int_literal.value = type->size;
	return process_expression(expr);
}

bool process_int_literal(ast_expression_t* expr)
{
	if (expr->data.int_literal.value < 0xFF)
	{
		expr->data.int_literal.type = uint8_type_spec;
	}
	else if (expr->data.int_literal.value < 0xFFFF)
	{
		expr->data.int_literal.type = uint16_type_spec;
	}
	else
	{
		expr->data.int_literal.type = uint32_type_spec;
	}
	return true;
}

bool process_expression(ast_expression_t* expr)
{
	if (!expr)
		return true;

	switch (expr->kind)
	{
		case expr_postfix_op:
		case expr_unary_op:
			return process_expression(expr->data.unary_op.expression);
		case expr_binary_op:
			if (expr->data.binary_op.operation == op_member_access || expr->data.binary_op.operation == op_ptr_member_access)
				return process_member_access_expression(expr);
			
			if (!process_expression(expr->data.binary_op.lhs))
				return false;
			return process_expression(expr->data.binary_op.rhs);
		case expr_int_literal:
			return process_int_literal(expr);
		case expr_assign:
			return process_assignment_expression(expr);
		case expr_identifier:
			return process_variable_reference(expr);
		case expr_condition:
			if (!process_expression(expr->data.condition.cond))
				return false;
			if (!process_expression(expr->data.condition.true_branch))
				return false;
			return process_expression(expr->data.condition.false_branch);
		case expr_func_call:
			return process_func_call(expr);
		case expr_sizeof:			
			return process_sizeof_expr(expr);
		case expr_null:
			break;
	}

	return true;
}

bool process_block_item(ast_block_item_t* block)
{
	if (!block)
		return true;

	if (block->kind == blk_smnt)
	{
		return process_statement(block->smnt);
	}
	else if (block->kind == blk_decl)
	{
		return process_declaration(block->decl);
	}	
	return false;
}

bool process_block_list(ast_block_item_t* blocks)
{
	ast_block_item_t* block = blocks;
	while (block)
	{
		if (!process_block_item(block))
		{
			idm_leave_block(_id_map);
			return false;
		}
		block = block->next;
	}
	return true;
}

bool process_for_statement(ast_statement_t* smnt)
{
	if (!process_declaration(smnt->data.for_smnt.init_decl))
		return false;
	if (!process_expression(smnt->data.for_smnt.init))
		return false;
	if (!process_expression(smnt->data.for_smnt.condition))
		return false;
	if (!process_expression(smnt->data.for_smnt.post))
		return false;
	if (!process_statement(smnt->data.for_smnt.statement))
		return false;
	return true;
}

bool process_goto_statement(ast_statement_t* smnt)
{
	phs_insert(_cur_func_ctx.goto_smnts, smnt);
	return true;
}

bool process_label_statement(ast_statement_t* smnt)
{
	if(sht_contains(_cur_func_ctx.labels, smnt->data.label_smnt.label))
	{
		_report_err(smnt->tokens.start, ERR_DUP_LABEL,
			"dupliate label '%s' in function '%s'",
			smnt->data.label_smnt.label, _cur_func_ctx.decl->name);
		return false;
	}
	sht_insert(_cur_func_ctx.labels, smnt->data.label_smnt.label, 0);

	return process_statement(smnt->data.label_smnt.smnt);
}

bool process_switch_statement(ast_statement_t * smnt)
{
	if (!smnt->data.switch_smnt.default_case && !smnt->data.switch_smnt.cases)
	{
		_report_err(smnt->tokens.start, ERR_INVALID_SWITCH,
			"switch statement has no case or default statement");
		return false;
	}

	ast_switch_case_data_t* case_data = smnt->data.switch_smnt.cases;
	while (case_data)
	{
		if (case_data->const_expr->kind != expr_int_literal)
		{
			_report_err(smnt->tokens.start, ERR_INVALID_SWITCH,
				"case must be a constant expression");
			return false;
		}

		if (case_data->next == NULL && case_data->statement == NULL)
		{
			/*last item must have statement

			This is fine:
			case 1:
			case 2:
				return;

			This is an error:
			case 1:
				return;
			case 2:
			*/
			_report_err(smnt->tokens.start, ERR_INVALID_SWITCH,
				"invalid case in switch");
			return false;
		}

		case_data = case_data->next;
	}

	return true;
}

bool process_return_statement(ast_statement_t* smnt)
{
	assert(_cur_func_ctx.decl);

	if (!process_expression(smnt->data.expr))
		return false;

	if (smnt->data.expr && smnt->data.expr->kind != expr_null)
	{
		if (!_can_convert_type(_cur_func_ctx.decl->return_type_ref->spec, _resolve_expr_type(smnt->data.expr)))
		{
			_report_err(smnt->tokens.start, ERR_INVALID_RETURN,
				"return value type does not match function declaration");
			return false;
		}
	}
	else if (_cur_func_ctx.decl->return_type_ref->spec->kind != type_void)
	{
		_report_err(smnt->tokens.start, ERR_INVALID_RETURN,
			"function must return a value");
		return false;
	}
	return true;
}

bool process_statement(ast_statement_t* smnt)
{
	if (!smnt)
		return true;

	switch (smnt->kind)
	{
	case smnt_expr:
		return process_expression(smnt->data.expr);
		break;
	case smnt_do:
	case smnt_while:
		if (!process_expression(smnt->data.while_smnt.condition))
			return false;
		if (!process_statement(smnt->data.while_smnt.statement))
			return false;
		break;
	case smnt_for:
	case smnt_for_decl:
	{
		idm_enter_block(_id_map);
		bool result = process_for_statement(smnt);
		idm_leave_block(_id_map);
		return result;
	}
	case smnt_if:
		if (!process_expression(smnt->data.if_smnt.condition))
			return false;
		if (!process_statement(smnt->data.if_smnt.true_branch))
			return false;
		if (!process_statement(smnt->data.if_smnt.false_branch))
			return false;
		break;
	case smnt_compound:
		idm_enter_block(_id_map);
		bool ret = process_block_list(smnt->data.compound.blocks);
		idm_leave_block(_id_map);
		break;
	case smnt_return:
		return process_return_statement(smnt);
	case smnt_switch:
		return process_switch_statement(smnt);
	case smnt_label:
		return process_label_statement(smnt);
	case smnt_goto:
		return process_goto_statement(smnt);
		break;
	}

	return true;
}

bool process_function_definition(ast_function_decl_t* decl)
{
	//set up function data
	idm_enter_function(_id_map, decl);
	_cur_func_ctx.decl = decl;
	_cur_func_ctx.labels = sht_create(64);
	_cur_func_ctx.goto_smnts = phs_create(64);
	
	bool ret = process_block_list(decl->blocks);

	//check that any goto statements reference valid labels
	phs_iterator_t it = phs_begin(_cur_func_ctx.goto_smnts);
	while (!phs_end(_cur_func_ctx.goto_smnts, &it))
	{
		ast_statement_t* goto_smnt = (ast_statement_t*)it.val;

		if (!sht_contains(_cur_func_ctx.labels, goto_smnt->data.goto_smnt.label))
		{
			_report_err(goto_smnt->tokens.start, ERR_UNKNOWN_LABEL,
				"goto statement references unknown label '%s'",
				goto_smnt->data.goto_smnt.label);

		}
		phs_next(_cur_func_ctx.goto_smnts, &it);
	}

	ht_destroy(_cur_func_ctx.goto_smnts);
	_cur_func_ctx.goto_smnts = NULL;
	ht_destroy(_cur_func_ctx.labels);
	_cur_func_ctx.labels = NULL;
	_cur_func_ctx.decl = NULL;

	idm_leave_function(_id_map);
	return ret;
}

static bool _is_fn_definition(ast_declaration_t* decl)
{
	assert(decl->kind == decl_func);
	return decl->data.func.blocks != NULL;
}

bool resolve_function_decl_types(ast_declaration_t* decl)
{
	ast_function_decl_t* func = &decl->data.func;

	//return type
	if (!_resolve_type_ref(func->return_type_ref))
		return false;
	
	ast_type_spec_t* ret_type = func->return_type_ref->spec;

	if (_is_fn_definition(decl) && 
		ret_type->kind != type_void && 
		ret_type->size == 0)
	{
		_report_err(decl->tokens.start, ERR_TYPE_INCOMPLETE,
			"function '%s' returns incomplete type '%s'",
			func->name, ret_type->user_type_spec->name);
		return false;
	}
	
	//parameter types
	ast_func_param_decl_t* param = func->first_param;
	while (param)
	{
		if (param->decl->data.var.type_ref->spec->kind == type_void)
		{
			_report_err(param->decl->tokens.start, ERR_INVALID_PARAMS,
				"function param '%s' of void type",
				param->decl->data.var.name);
			return false;
		}

		if (!_resolve_type_ref(param->decl->data.var.type_ref))
			return false;

		ast_type_spec_t* param_type = param->decl->data.var.type_ref->spec;
		
		if (_is_fn_definition(decl) && 
			param_type->kind != type_void &&
			param_type->size == 0)
		{
			_report_err(param->decl->tokens.start, ERR_TYPE_INCOMPLETE,
				"function param '%s' returns incomplete type '%s'",
				param->decl->data.var.name, param_type->user_type_spec->name);
			return false;
		}
		
		param = param->next;
	}
	return true;
}

static bool _compare_func_decls(ast_declaration_t* exist, ast_declaration_t* func)
{
	if (exist->data.func.return_type_ref->spec != func->data.func.return_type_ref->spec)
	{
		_report_err(func->tokens.start,
			ERR_INVALID_PARAMS,
			"differing return type in declaration of function '%s'. Expected '%s'",
			ast_declaration_name(func), ast_type_ref_name(exist->data.func.return_type_ref));
		return false;
	}

	if (exist->data.func.param_count != func->data.func.param_count)
	{
		_report_err(func->tokens.start, 
			ERR_INVALID_PARAMS,
			"incorrect number of params in declaration of function '%s'. Expected %d",
			ast_declaration_name(func), exist->data.func.param_count);
		return false;
	}

	ast_func_param_decl_t* p_exist = exist->data.func.first_param;
	ast_func_param_decl_t* p_func = func->data.func.first_param;

	int p_count = 1;
	while (p_exist && p_func)
	{
		if (p_exist->decl->data.var.type_ref->spec != _resolve_type(p_func->decl->data.var.type_ref->spec))
		{
			_report_err(func->tokens.start,
				ERR_INVALID_PARAMS,
				"conflicting type in param %d of declaration of function '%s'. Expected '%s'",
				p_count, ast_declaration_name(func), ast_type_ref_name(p_exist->decl->data.var.type_ref));
			return false;
		}

		p_exist = p_exist->next;
		p_func = p_func->next;
		p_count++;
	}

	return p_exist == NULL && p_func == NULL;
}

proc_decl_result process_function_decl(ast_declaration_t* decl)
{
	const char* name = decl->data.func.name;

	ast_declaration_t* var = idm_find_decl(_id_map, name, decl_var);
	if (var)
	{
		_report_err(decl->tokens.start, ERR_DUP_SYMBOL,
			"declaration of function '%s' shadows variable at",
			name);
		return proc_decl_error;
	}

	if (!resolve_function_decl_types(decl))
		return proc_decl_error;

	ast_declaration_t* exist = idm_find_decl(_id_map, name, decl_func);
	if (exist)
	{
		if (_is_fn_definition(exist) && _is_fn_definition(decl))
		{
			//multiple definition
			_report_err(decl->tokens.start, ERR_DUP_SYMBOL,
				"redefinition of function '%s'", name);
			return proc_decl_error;
		}
		
		if (!_compare_func_decls(exist, decl))
			return proc_decl_error;

		if (!_is_fn_definition(exist) && _is_fn_definition(decl))
		{
			//update definition
			//exist->data.func
			idm_update_decl(_id_map, decl);
		}
	}
	else
	{
		idm_add_id(_id_map, decl);
	}

	return _is_fn_definition(decl) ? proc_decl_new_def : proc_decl_ignore;
}

/*
Multiple declarations are allowed
Only a single definition is permitted
Type cannot change
*/
proc_decl_result process_global_var_decl(ast_declaration_t* decl)
{
	const char* name = decl->data.var.name;

	ast_declaration_t* fn = idm_find_decl(_id_map, name, decl_func);
	if (fn)
	{
		_report_err(decl->tokens.start, ERR_DUP_SYMBOL,
			"global var declaration of '%s' shadows function at",
			name);
		return proc_decl_error;
	}

	if (!_resolve_type_ref(decl->data.var.type_ref))
	{		
		return proc_decl_error;
	}

	ast_type_spec_t* type = decl->data.var.type_ref->spec;
	ast_declaration_t* exist = idm_find_decl(_id_map, name, decl_var);
	if (exist)
	{		
		//multiple declarations are allowed
		if (!type || exist->data.var.type_ref->spec != type)
		{
			//different types
			_report_err(decl->tokens.start, ERR_DUP_SYMBOL,
				"incompatible redeclaration of global var '%s'",
				name);
			return proc_decl_error;
		}

		if (exist->data.var.expr && decl->data.var.expr)
		{
			//multiple definitions
			_report_err(decl->tokens.start, ERR_DUP_SYMBOL,
				"redefinition of global var '%s'",
				name);
			return proc_decl_error;
		}

		if (!exist->data.var.expr && decl->data.var.expr)
		{
			//update definition
			exist->data.var.expr = decl->data.var.expr;
			decl->data.var.expr = NULL;
		}
		return proc_decl_ignore;
	}

	if (type->size == 0)
	{
		_report_err(decl->tokens.start, ERR_TYPE_INCOMPLETE,
			"global var '%s' uses incomplete type '%s'",
			name, ast_type_ref_name(decl->data.var.type_ref));
	}
	idm_add_id(_id_map, decl);
	return proc_decl_new_def;
}

static void _add_fn_decl(valid_trans_unit_t* tl, ast_declaration_t* fn)
{
	tl_decl_t* tl_decl = (tl_decl_t*)malloc(sizeof(tl_decl_t));
	memset(tl_decl, 0, sizeof(tl_decl_t));
	tl_decl->decl = fn;
	tl_decl->next = tl->fn_decls;
	tl->fn_decls = tl_decl;
}

static void _add_var_decl(valid_trans_unit_t* tl, ast_declaration_t* fn)
{
	tl_decl_t* tl_decl = (tl_decl_t*)malloc(sizeof(tl_decl_t));
	memset(tl_decl, 0, sizeof(tl_decl_t));
	tl_decl->decl = fn;
	tl_decl->next = tl->var_decls;
	tl->var_decls = tl_decl;
}

static void _add_type_decl(valid_trans_unit_t* tl, ast_declaration_t* fn)
{
	tl_decl_t* tl_decl = (tl_decl_t*)malloc(sizeof(tl_decl_t));
	memset(tl_decl, 0, sizeof(tl_decl_t));
	tl_decl->decl = fn;
	tl_decl->next = tl->type_decls;
	tl->type_decls = tl_decl;
}

valid_trans_unit_t* sem_analyse(ast_trans_unit_t* ast)
{
	types_init();
	_id_map = idm_create();

	_cur_func_ctx.decl = NULL;
	_cur_func_ctx.labels = NULL;

	hash_table_t* types = phs_create(128);

	valid_trans_unit_t* tl = (valid_trans_unit_t*)malloc(sizeof(valid_trans_unit_t));
	memset(tl, 0, sizeof(valid_trans_unit_t));
	tl->ast = ast;

	ast_declaration_t* decl = ast->decls;
	while (decl)
	{
		ast_declaration_t* next = decl->next;
		if (decl->kind == decl_var)
		{
			proc_decl_result result = process_global_var_decl(decl);

			if (result == proc_decl_error)
				return NULL;
			if (result == proc_decl_new_def)
			{
				_add_var_decl(tl, decl);
			}
		}
		else if (decl->kind == decl_func)
		{
			proc_decl_result result = process_function_decl(decl);
			if (result == proc_decl_error)
				return NULL;
			if (result == proc_decl_new_def)
			{
				_add_fn_decl(tl, decl);

				if (decl->data.func.blocks)
				{
					if (!process_function_definition(&decl->data.func))
					{
						return NULL;
					}
					
				}
			}
		}
		else if (decl->kind == decl_type)
		{
			ast_type_spec_t* type = _resolve_type(decl->data.type);

			if (!ht_contains(types, type))
			{
				phs_insert(types, type);
				_add_type_decl(tl, decl);
			}
		}
		decl = next;
	}
	ht_destroy(types);
	idm_destroy(_id_map);
	_id_map = NULL;	
	return tl;
}

void tl_destroy(valid_trans_unit_t* tl)
{
	if (!tl)
		return;
	
	ast_destory_translation_unit(tl->ast);
	free(tl);
}