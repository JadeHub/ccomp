#include "validate.h"
#include "diag.h"
#include "var_set.h"
#include "decl_map.h"
#include "nps.h"

#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/*built in types */
/* {{token_range_t}, kind, size, flags, user_type_spec} */
static ast_type_spec_t _void_type =		{ {NULL, NULL}, type_void,		0, NULL };
static ast_type_spec_t _char_type =		{ {NULL, NULL}, type_int8,		1, NULL };
static ast_type_spec_t _uchar_type =	{ {NULL, NULL}, type_uint8,		1, NULL };
static ast_type_spec_t _short_type =	{ {NULL, NULL}, type_int16,		2, NULL };
static ast_type_spec_t _ushort_type =	{ {NULL, NULL}, type_uint16,	2, NULL };
static ast_type_spec_t _int_type =		{ {NULL, NULL}, type_int32,		4, NULL };
static ast_type_spec_t _uint_type =		{ {NULL, NULL}, type_uint32,	4, NULL };

static identfier_map_t* _id_map;
static ast_function_decl_t* _cur_func = NULL;

typedef enum
{
	proc_decl_new_def,
	proc_decl_ignore,
	proc_decl_error
}proc_decl_result;

proc_decl_result process_function_decl(ast_declaration_t* decl);
bool process_expression(ast_expression_t* expr);
bool process_statement(ast_statement_t* smnt);

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
		member->type = _resolve_type(member->type);
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
		decl->data.const_val.type = &_int_type;
		decl->data.const_val.expr = member->const_value;
		decl->data.const_val.expr->data.int_literal.type = &_int_type;

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

static void _add_user_type(ast_type_spec_t* typeref)
{
	if (typeref->user_type_spec->kind == user_type_enum)
		_register_enum_constants(typeref->user_type_spec);
	else
		_resolve_struct_member_types(typeref->user_type_spec);
	idm_add_tag(_id_map, typeref);
}

static ast_type_spec_t* _resolve_type(ast_type_spec_t* typeref)
{
	switch (typeref->kind)
	{
	case type_void:
		ast_destroy_type_spec(typeref);
		return &_void_type;
	case type_int8:
		ast_destroy_type_spec(typeref);
		return &_char_type;
	case type_uint8:
		ast_destroy_type_spec(typeref);
		return &_uchar_type;
	case type_int16:
		ast_destroy_type_spec(typeref);
		return &_short_type;
	case type_uint16:
		ast_destroy_type_spec(typeref);
		return &_ushort_type;
	case type_int32:
		ast_destroy_type_spec(typeref);
		return &_int_type;
	case type_uint32:
		ast_destroy_type_spec(typeref);
		return &_uint_type;
	case type_user:
		break;
	}

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
	if (_user_type_is_definition(typeref->user_type_spec))
	{
		ast_type_spec_t* exist = idm_find_block_tag(_id_map, typeref->user_type_spec->name);
		if (typeref == exist)
			return exist;

		if (exist)
		{
			if (exist->user_type_spec->kind != typeref->user_type_spec->kind)
			{
				_report_err(typeref->tokens.start, ERR_DUP_TYPE_DEF,
					"redefinition of %s '%s' changes type to %s",
					user_type_kind_name(exist->user_type_spec->kind),
					typeref->user_type_spec->name,
					user_type_kind_name(typeref->user_type_spec->kind));
				return NULL;
			}

			if (_user_type_is_definition(exist->user_type_spec))
			{
				//multiple definitions
				_report_err(typeref->tokens.start, ERR_DUP_TYPE_DEF,
					"redefinition of %s '%s'",
					user_type_kind_name(typeref->user_type_spec->kind),
					typeref->user_type_spec->name);
				return NULL;
			}
			//update the existing declaration
			exist->size = typeref->size;
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
				_report_err(typeref->tokens.start, ERR_DUP_TYPE_DEF,
					"redefinition of %s '%s' changes type to %s",
					user_type_kind_name(exist->user_type_spec->kind),
					typeref->user_type_spec->name,
					user_type_kind_name(typeref->user_type_spec->kind));
				return NULL;
			}
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

	ast_type_spec_t* type = _resolve_type(var->type);
	if (!type || type->size == 0)
	{
		_report_err(decl->tokens.start, ERR_TYPE_INCOMPLETE, "var %s is of incomplete type",
			ast_declaration_name(decl));
		return false;
	}
	var->type = type;
	idm_add_id(_id_map, decl);
	_cur_func->required_stack_size += var->type->size;

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
		return _resolve_type(&decl->data.type);		
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
		ast_type_spec_t* type = _resolve_type(decl->data.var.type);
		if (!type || type->size == 0)
		{
			_report_err(decl->tokens.start, ERR_TYPE_INCOMPLETE, "var %s is of incomplete type",
				ast_declaration_name(decl));
			return false;
		}
		decl->data.var.type = type;
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
		return _resolve_expr_type(expr->data.unary_op.expression);
	case expr_binary_op:
	{
		if (expr->data.binary_op.operation == op_member_access)
		{
			// a.b (lhs.rhs)
			ast_type_spec_t* user_type_spec = _resolve_expr_type(expr->data.binary_op.lhs);
			assert(user_type_spec);
			ast_type_spec_t* member_spec = ast_find_struct_member(user_type_spec->user_type_spec, expr->data.binary_op.rhs->data.var_reference.name)->type;
			return _resolve_type(member_spec);
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
			return _resolve_type(decl->data.var.type);
		return NULL;
	}
	
	case expr_condition:
		//if (!process_expression(expr->data.condition.cond))
		//	return false;
		//if (!process_expression(expr->data.condition.true_branch))
		//	return false;
		//return process_expression(expr->data.condition.false_branch);
		break;
	case expr_func_call:
	{
		ast_declaration_t* decl = idm_find_decl(_id_map, expr->data.var_reference.name, decl_func);
		if (decl)
			return _resolve_type(decl->data.func.return_type);
		return NULL;
	}
	case expr_sizeof:
		return &_int_type;

	case expr_null:
		return &_void_type;
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
	ast_declaration_t* func_param = decl->data.func.params;

	int p_count = 1;
	while (call_param && func_param)
	{
		if (!process_expression(call_param))
			return false;

		if (!_can_convert_type(func_param->data.var.type, _resolve_expr_type(call_param)))
		{
			_report_err(expr->tokens.start,
				ERR_INVALID_PARAMS,
				"conflicting type in param %d of call to function '%s'. Expected '%s'",
				p_count, ast_declaration_name(decl), ast_type_name(func_param->data.var.type));
			return false;
		}

		call_param = call_param->next;
		func_param = func_param->next;
		p_count++;
	}

	return true;
}

bool process_member_access_expression(ast_expression_t* expr)
{
	// a.b (lhs.rhs)
	if (!process_expression(expr->data.binary_op.lhs))
		return false;

	ast_type_spec_t* user_type_spec = _resolve_expr_type(expr->data.binary_op.lhs);
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
		expr->data.int_literal.type = _resolve_type(&_char_type);
	}
	else if (expr->data.int_literal.value < 0xFFFF)
	{
		expr->data.int_literal.type = _resolve_type(&_short_type);
	}
	else
	{
		expr->data.int_literal.type = _resolve_type(&_int_type);
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
			if (expr->data.binary_op.operation == op_member_access)
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

bool process_return_statement(ast_statement_t* smnt)
{
	assert(_cur_func);

	if (!process_expression(smnt->data.expr))
		return false;

	if (smnt->data.expr && smnt->data.expr->kind != expr_null)
	{
		if (!_can_convert_type(_cur_func->return_type, _resolve_expr_type(smnt->data.expr)))
		{
			_report_err(smnt->tokens.start, ERR_INVALID_RETURN,
				"return value type does not match function declaration");
			return false;
		}
	}
	else if (_cur_func->return_type->kind != type_void)
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
	}

	return true;
}

bool process_function_definition(ast_function_decl_t* decl)
{
	_cur_func = decl;
	bool ret = process_block_list(decl->blocks);
	_cur_func = NULL;
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
	ast_type_spec_t* ret_type = _resolve_type(func->return_type);
	if (!ret_type)
		return false;
	
	func->return_type = ret_type;
	
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
	ast_declaration_t* param = func->params;
	while (param)
	{
		if (param->data.var.type->kind == type_void)
		{
			_report_err(param->tokens.start, ERR_INVALID_PARAMS,
				"function param '%s' of void type",
				param->data.var.name);
			return false;
		}

		ast_type_spec_t* param_type = _resolve_type(param->data.var.type);
		if (!param_type)
			return false;
		param->data.var.type = param_type;

		if (_is_fn_definition(decl) && 
			param_type->kind != type_void &&
			param_type->size == 0)
		{
			_report_err(param->tokens.start, ERR_TYPE_INCOMPLETE,
				"function param '%s' returns incomplete type '%s'",
				param->data.var.name, param_type->user_type_spec->name);
			return false;
		}
		
		param = param->next;
	}
	return true;
}

static bool _compare_func_decls(ast_declaration_t* exist, ast_declaration_t* func)
{
	if (exist->data.func.return_type != _resolve_type(func->data.func.return_type))
	{
		_report_err(func->tokens.start,
			ERR_INVALID_PARAMS,
			"differing return type in declaration of function '%s'. Expected '%s'",
			ast_declaration_name(func), ast_type_name(exist->data.func.return_type));
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

	ast_declaration_t* p_exist = exist->data.func.params;
	ast_declaration_t* p_func = func->data.func.params;

	int p_count = 1;
	while (p_exist && p_func)
	{
		if (p_exist->data.var.type != _resolve_type(p_func->data.var.type))
		{
			_report_err(func->tokens.start,
				ERR_INVALID_PARAMS,
				"conflicting type in param %d of declaration of function '%s'. Expected '%s'",
				p_count, ast_declaration_name(func), ast_type_name(p_exist->data.var.type));
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
			if (!resolve_function_decl_types(decl))
				return proc_decl_error;

			idm_update_decl(_id_map, decl);
		}
	}
	else
	{
		if (!resolve_function_decl_types(decl))
			return proc_decl_error;
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

	ast_declaration_t* exist = idm_find_decl(_id_map, name, decl_var);
	if (exist)
	{
		//multiple declarations are allowed
		if (exist->data.var.type != _resolve_type(decl->data.var.type))
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

	ast_type_spec_t* type = _resolve_type(decl->data.var.type);
	if (!type)
	{
		return proc_decl_error;
	}

	if (type->size == 0)
	{
		_report_err(decl->tokens.start, ERR_TYPE_INCOMPLETE,
			"global var '%s' uses incomplete type '%s'",
			name, type->user_type_spec->name);
		return proc_decl_error;
	}

	decl->data.var.type = type;
	idm_add_id(_id_map, decl);
	return proc_decl_new_def;
}

valid_trans_unit_t* tl_validate(ast_trans_unit_t* ast)
{
	_id_map = idm_create();

	ast_declaration_t* vars = NULL;
	ast_declaration_t* fns = NULL;
	
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
				decl->next = vars;
				vars = decl;
			}
		}
		else if (decl->kind == decl_func)
		{
			proc_decl_result result = process_function_decl(decl);
			if (result == proc_decl_error)
				return NULL;
			if (result == proc_decl_new_def)
			{
				decl->next = fns;
				fns = decl;
				if (decl->data.func.blocks)
				{
					idm_enter_function(_id_map, &decl->data.func);
					if (!process_function_definition(&decl->data.func))
					{
						return NULL;
					}
					idm_leave_function(_id_map);
				}
			}
		}
		else if (decl->kind == decl_type)
		{
			_resolve_type(&decl->data.type);
		}
		decl = next;
	}

	valid_trans_unit_t* result = (valid_trans_unit_t*)malloc(sizeof(valid_trans_unit_t));
	memset(result, 0, sizeof(valid_trans_unit_t));
	result->ast = ast;
	result->functions = fns;
	result->variables = vars;
	result->identifiers = _id_map;
	_id_map = NULL;

	return result;
}

void tl_destroy(valid_trans_unit_t* tl)
{
	if (!tl)
		return;
	idm_destroy(tl->identifiers);
	ast_destory_translation_unit(tl->ast);
	free(tl);
}