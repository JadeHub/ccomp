#include "sema.h"
#include "sema_internal.h"
#include "sema_types.h"
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

identfier_map_t* sema_id_map()
{
	return _id_map; 
}

typedef struct
{
	ast_declaration_t* decl;

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
bool process_statement(ast_statement_t* smnt);

static bool _report_err(token_t* tok, int err, const char* format, ...)
{
	char buff[512];

	va_list args;
	va_start(args, format);
	vsnprintf(buff, 512, format, args);
	va_end(args);

	diag_err(tok, err, buff);
	return false;
}

static void _resolve_struct_member_types(ast_user_type_spec_t* user_type_spec)
{
	assert(user_type_spec->kind != user_type_enum);
	ast_struct_member_t* member = user_type_spec->data.struct_members;
	while (member)
	{
		sema_resolve_type_ref(member->decl->type_ref);
		member = member->next;
	}
}

static bool process_expression(ast_expression_t* expr)
{
	if (!expr)
		return true;

	expr_result_t result = sema_process_expression(expr);

	return !result.failure;
}

/*
Create asl_declaration_t objects for each item in the enum
*/
static bool _register_enum_constants(ast_user_type_spec_t* user_type_spec)
{
	assert(user_type_spec->kind == user_type_enum);
	ast_enum_member_t* member = user_type_spec->data.enum_members;
	int_val_t next_val = int_val_zero();
	
	while (member)
	{
		if (member->value)
		{
			if (!sema_is_const_int_expr(member->value))
			{
				//err
				return false;
			}
			next_val = sema_fold_const_int_expr(member->value);
		}

		idm_add_enum_val(_id_map, member->name, next_val);

		member = member->next;
		next_val = int_val_inc(next_val);
	}
	return true;
}

static bool _user_type_is_definition(ast_user_type_spec_t* type)
{
	if (type->kind == user_type_enum)
		return type->data.enum_members != NULL;
	return type->data.struct_members != NULL;
}

static uint32_t _calc_user_type_size(ast_type_spec_t* typeref)
{
	if (typeref->data.user_type_spec->kind == user_type_enum)
		return 4;

	uint32_t result = 0;
	ast_struct_member_t* member = typeref->data.user_type_spec->data.struct_members;
	while (member)
	{
		result += member->decl->type_ref->spec->size;
		member = member->next;
	}
	return result;
}

static void _add_user_type(ast_type_spec_t* typeref)
{
	if (typeref->data.user_type_spec->kind == user_type_enum)
		_register_enum_constants(typeref->data.user_type_spec);
	else
		_resolve_struct_member_types(typeref->data.user_type_spec);
	typeref->size = _calc_user_type_size(typeref);
	idm_add_tag(_id_map, typeref);
}

ast_type_spec_t* sema_resolve_type(ast_type_spec_t* spec, token_t* start)
{
	if (spec->kind == type_alias)
	{
		ast_declaration_t* decl = idm_find_decl(_id_map, spec->data.alias);
		if (!decl)
		{
			_report_err(start, ERR_UNKNOWN_TYPE, "type alias %s unknown", spec->data.alias);
			return NULL;
		}
		if (decl->kind != decl_type)
		{
			_report_err(start, ERR_INCOMPATIBLE_TYPE, "%s does not name a type alias", spec->data.alias);
			return NULL;
		}

		spec = decl->type_ref->spec;
	}

	if (spec->kind == type_ptr)
	{
		spec->data.ptr_type = sema_resolve_type(spec->data.ptr_type, start);
		return spec;
	}

	if (spec->kind != type_user)
		return spec;

	/*
	structs, unions & enums

	if spec is a definition
		Look for a type with the same name in the current lexical block
			If declaration found: Update with definition
			If definition found: Error duplicate definition
			If not found: Add to id_map
	else if spec is a declaration
		Look for a type with the same name in any lexical block
		If found: return
		If not found: Add to id_map
	*/
	token_t* loc = spec->data.user_type_spec->tokens.start;
	if (_user_type_is_definition(spec->data.user_type_spec))
	{
		//ignore anonymous types
		if (strlen(spec->data.user_type_spec->name) == 0)
		{
			_add_user_type(spec);
			return spec;
		}

		ast_type_spec_t* exist = idm_find_block_tag(_id_map, spec->data.user_type_spec->name);
		if (spec == exist)
			return exist;

		if (exist)
		{
			if (exist->data.user_type_spec->kind != spec->data.user_type_spec->kind)
			{
				_report_err(loc, ERR_DUP_TYPE_DEF,
					"redefinition of %s '%s' changes type to %s",
					user_type_kind_name(exist->data.user_type_spec->kind),
					spec->data.user_type_spec->name,
					user_type_kind_name(spec->data.user_type_spec->kind));
				return NULL;
			}

			if (_user_type_is_definition(exist->data.user_type_spec))
			{
				//multiple definitions
				_report_err(loc, ERR_DUP_TYPE_DEF,
					"redefinition of %s '%s'",
					user_type_kind_name(spec->data.user_type_spec->kind),
					spec->data.user_type_spec->name);
				return NULL;
			}
			//update the existing declaration
			if (spec->data.user_type_spec->kind == user_type_enum)
			{
				exist->data.user_type_spec->data.enum_members = spec->data.user_type_spec->data.enum_members;
				spec->data.user_type_spec->data.enum_members = NULL;
				_register_enum_constants(exist->data.user_type_spec);
			}
			else
			{
				//update definition
				exist->data.user_type_spec->data.struct_members = spec->data.user_type_spec->data.struct_members;
				spec->data.user_type_spec->data.struct_members = NULL;
				_resolve_struct_member_types(exist->data.user_type_spec);
			}
			exist->size = _calc_user_type_size(exist);
			ast_destroy_type_spec(spec);
			spec = exist;
		}
		else
		{
			_add_user_type(spec);
		}
	}
	else
	{
		//declaration
		ast_type_spec_t* exist = idm_find_tag(_id_map, spec->data.user_type_spec->name);
		if (exist)
		{
			if (exist->data.user_type_spec->kind != spec->data.user_type_spec->kind)
			{
				_report_err(loc, ERR_DUP_TYPE_DEF,
					"redefinition of %s '%s' changes type to %s",
					user_type_kind_name(exist->data.user_type_spec->kind),
					spec->data.user_type_spec->name,
					user_type_kind_name(spec->data.user_type_spec->kind));
				return NULL;
			}
			ast_destroy_type_spec(spec);
			spec = exist;
		}
		else
		{
			_add_user_type(spec);
		}
	}

	//treat enums as int32_t
	if (ast_type_is_enum(spec))
		return int32_type_spec;

	return spec;
}

bool sema_resolve_type_ref(ast_type_ref_t* ref)
{
	assert(ref->spec);

	ast_type_spec_t* spec = sema_resolve_type(ref->spec, ref->tokens.start);
	if (spec)
	{
		ref->spec = spec;
		return true;
	}
	return false;
}

bool sema_is_same_func_params(ast_func_params_t* lhs, ast_func_params_t* rhs)
{
	if (lhs->param_count != rhs->param_count ||
		lhs->ellipse_param != rhs->ellipse_param)
		return false;

	ast_func_param_decl_t* lhs_param = lhs->first_param;
	ast_func_param_decl_t* rhs_param = rhs->first_param;

	while (lhs_param && rhs_param)
	{
		if (!sema_is_same_type(lhs_param->decl->type_ref->spec, rhs_param->decl->type_ref->spec))
			return false;
		lhs_param = lhs_param->next;
		rhs_param = rhs_param->next;
	}
	return lhs_param == rhs_param;
}

bool sema_is_same_func_sig(ast_func_sig_type_spec_t* lhs, ast_func_sig_type_spec_t* rhs)
{
	if (!sema_is_same_type(lhs->ret_type, rhs->ret_type))
		return false;

	return sema_is_same_func_params(lhs->params, rhs->params);
}

//expect that lhs and rhs have been 'resolved'
bool sema_is_same_type(ast_type_spec_t* lhs, ast_type_spec_t* rhs)
{
	if (lhs->kind != rhs->kind)
		return false;

	if (lhs->kind == type_ptr)
	{
		return sema_is_same_type(lhs->data.ptr_type, rhs->data.ptr_type);
	}

	if (lhs->kind == type_func_sig)
	{
		return sema_is_same_func_sig(lhs->data.func_sig_spec, rhs->data.func_sig_spec);
	}

	return lhs == rhs;
}

bool sema_can_convert_type(ast_type_spec_t* target, ast_type_spec_t* type)
{
	if (!type)
		return false;

	//are they exactly the same type?
	if (sema_is_same_type(target, type))
		return true;

	//integer promotion
	if (ast_type_is_int(target) && ast_type_is_int(type) && target->size >= type->size)
	{
		return true;
	}

	//are they pointing at convertable types?
	if (target->kind == type_ptr && type->kind == type_ptr)
		return sema_can_convert_type(target->data.ptr_type, type->data.ptr_type);

	//are the compatible functionn sigs (?)
	if (target->kind == type_func_sig && type->kind == type_func_sig)
	{

	}

	return false;
}

static size_t _process_array_alloc_size(ast_declaration_t* decl)
{
	ast_expression_list_t* array_sz = decl->array_dimensions;
	size_t result = 0;

	while (array_sz)
	{
		ast_expression_t* expr = array_sz->expr;

		if (!sema_is_const_int_expr(expr))
		{
			_report_err(expr->tokens.start, ERR_TYPE_INCOMPLETE,
				"array size must be constant expression",
				ast_declaration_name(decl));
			return 0;
			
		}
		int_val_t expr_val = sema_fold_const_int_expr(expr);
		if(!result)
			result = expr_val.v.uint64 * decl->type_ref->spec->data.ptr_type->size;
		else
			result *= expr_val.v.uint64;
		array_sz = array_sz->next;
	}

	if (result == 0)
	{
		_report_err(decl->tokens.start, ERR_TYPE_INCOMPLETE,
			"array size unknown",
			ast_declaration_name(decl));
		return 0;
	}
	return result;
}

bool process_variable_declaration(ast_declaration_t* decl)
{
	ast_declaration_t* existing = idm_find_block_decl(_id_map, decl->name);
	if (existing && existing->kind == decl_var)
	{
		return _report_err(decl->tokens.start, ERR_DUP_VAR,
			"variable %s already declared at",
			ast_declaration_name(decl));
	}

	if(!sema_resolve_type_ref(decl->type_ref) || decl->type_ref->spec->size == 0)
	{
		return _report_err(decl->tokens.start, ERR_TYPE_INCOMPLETE,
			"var %s is of incomplete type",
			ast_declaration_name(decl));
	}

	if (ast_is_array_decl(decl))
	{
		decl->sema.alloc_size = _process_array_alloc_size(decl);
		if (decl->sema.alloc_size == 0)
			return false;
	}
	else
	{
		decl->sema.alloc_size = decl->type_ref->spec->size;
	}

	if (decl->data.var.init_expr)
	{
		expr_result_t result = sema_process_expression(decl->data.var.init_expr);
		if (result.failure)
			return false;

		if (result.result_type->kind == type_func_sig)
		{
			//implicit conversion to function pointer
			result.result_type = ast_make_ptr_type(result.result_type);
		}

		ast_type_spec_t* init_type = result.result_type;

		if (!sema_can_convert_type(decl->type_ref->spec, init_type))
		{
			return _report_err(decl->data.var.init_expr->tokens.start,
				ERR_INCOMPATIBLE_TYPE,
				"assignment to incompatible type. expected %s",
				ast_type_name(decl->type_ref->spec));
		}
	}

	idm_add_decl(_id_map, decl);
	_cur_func_ctx.decl->data.func.sema.required_stack_size += decl->sema.alloc_size;

	return true;
}

bool process_type_decl(ast_declaration_t* decl)
{
	if (!sema_resolve_type_ref(decl->type_ref))
		return false;
	
	if (strlen(decl->name))
	{
		ast_declaration_t* existing = idm_find_block_decl(_id_map, decl->name);
		if (existing)
		{
			return _report_err(decl->tokens.start, ERR_DUP_TYPE_DEF,
				"typedef forces redefinition of %s", decl->name);
		}

		idm_add_decl(_id_map, decl);
	}

	return true;
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
		return process_type_decl(decl);
	}
	return true;
}



bool process_block_item(ast_block_item_t* block)
{
	if (!block)
		return true;

	if (block->kind == blk_smnt)
	{
		return process_statement(block->data.smnt);
	}
	else if (block->kind == blk_decl)
	{
		ast_declaration_t* decl = block->data.decls.first;
		while (decl)
		{
			if (!process_declaration(decl))
				return false;
			decl = decl->next;
		}
		return true;
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
	/* todo
	* The declaration part of a for statement shall only declare identifiers for objects having
storage class auto or register
	*/

	ast_declaration_t* decl = smnt->data.for_smnt.decls.first;
	while (decl)
	{
		if (!process_declaration(decl))
			return false;
		decl = decl->next;
	}

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
		return _report_err(smnt->tokens.start, ERR_DUP_LABEL,
			"dupliate label '%s' in function '%s'",
			smnt->data.label_smnt.label, _cur_func_ctx.decl->name);
	}
	sht_insert(_cur_func_ctx.labels, smnt->data.label_smnt.label, 0);

	return process_statement(smnt->data.label_smnt.smnt);
}

bool process_switch_statement(ast_statement_t * smnt)
{
	if (!smnt->data.switch_smnt.default_case && !smnt->data.switch_smnt.cases)
	{
		return _report_err(smnt->tokens.start, ERR_INVALID_SWITCH,
			"switch statement has no case or default statement");
	}

	if (!process_expression(smnt->data.switch_smnt.expr))
		return false;

	ast_switch_case_data_t* case_data = smnt->data.switch_smnt.cases;
	while (case_data)
	{
		//todo - fold const_expr?

		if (!process_expression(case_data->const_expr))
			return false;

		if (case_data->const_expr->kind != expr_int_literal)
		{
			return _report_err(smnt->tokens.start, ERR_INVALID_SWITCH,
				"case must be a constant expression");
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
			return _report_err(smnt->tokens.start, ERR_INVALID_SWITCH,
				"invalid case in switch");
		}

		case_data = case_data->next;
	}

	return true;
}

bool process_return_statement(ast_statement_t* smnt)
{
	assert(_cur_func_ctx.decl);

	expr_result_t result = sema_process_expression(smnt->data.expr);
	if (result.failure)
		return false;
	
	if (smnt->data.expr && smnt->data.expr->kind != expr_null)
	{
		if (!sema_can_convert_type(ast_func_decl_return_type(_cur_func_ctx.decl), result.result_type))
		{
			return _report_err(smnt->tokens.start, ERR_INVALID_RETURN,
				"return value type does not match function declaration");
		}
	}
	else if (_cur_func_ctx.decl->type_ref->spec->kind != type_void)
	{
		return _report_err(smnt->tokens.start, ERR_INVALID_RETURN,
			"function must return a value");
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
	{
		idm_enter_block(_id_map);
		bool ret = process_block_list(smnt->data.compound.blocks);
		idm_leave_block(_id_map);
		return ret;
	}
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

bool process_function_definition(ast_declaration_t* decl)
{
	//set up function data
	idm_enter_function(_id_map, decl->type_ref->spec->data.func_sig_spec->params);

	_cur_func_ctx.decl = decl;
	_cur_func_ctx.labels = sht_create(64);
	_cur_func_ctx.goto_smnts = phs_create(64);
	
	bool ret = process_block_list(decl->data.func.blocks);

	//check that any goto statements reference valid labels
	phs_iterator_t it = phs_begin(_cur_func_ctx.goto_smnts);
	while (!phs_end(_cur_func_ctx.goto_smnts, &it))
	{
		ast_statement_t* goto_smnt = (ast_statement_t*)it.val;

		if (!sht_contains(_cur_func_ctx.labels, goto_smnt->data.goto_smnt.label))
		{
			return _report_err(goto_smnt->tokens.start, ERR_UNKNOWN_LABEL,
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
	ast_func_sig_type_spec_t* fsig = decl->type_ref->spec->data.func_sig_spec;

	//return type
	fsig->ret_type = sema_resolve_type(fsig->ret_type, decl->tokens.start);
	if (!fsig->ret_type)
		return false;
	
	if (_is_fn_definition(decl) && 
		fsig->ret_type->kind != type_void &&
		fsig->ret_type->size == 0)
	{
		return _report_err(decl->tokens.start, ERR_TYPE_INCOMPLETE,
			"function '%s' returns incomplete type '%s'",
			decl->name, ast_type_name(fsig->ret_type));
	}
	
	//parameter types
	ast_func_param_decl_t* param = fsig->params->first_param;
	while (param)
	{
		if (param->decl->type_ref->spec->kind == type_void)
		{
			return _report_err(param->decl->tokens.start, ERR_INVALID_PARAMS,
				"function param '%s' of void type",
				param->decl->name);
		}

		if (!sema_resolve_type_ref(param->decl->type_ref))
			return false;

		ast_type_spec_t* param_type = param->decl->type_ref->spec;
		
		if (_is_fn_definition(decl) && 
			param_type->kind != type_void &&
			param_type->size == 0)
		{
			return _report_err(param->decl->tokens.start, ERR_TYPE_INCOMPLETE,
				"function param '%s' of incomplete type '%s'",
				param->decl->name, param_type->data.user_type_spec->name);
		}
		
		param = param->next;
	}
	return true;
}

proc_decl_result process_function_decl(ast_declaration_t* decl)
{
	const char* name = decl->name;

	ast_declaration_t* exist = idm_find_decl(_id_map, name);
	if (exist && exist->kind == decl_var)
	{
		_report_err(decl->tokens.start, ERR_DUP_SYMBOL,
			"declaration of function '%s' shadows variable at",
			name);
		return proc_decl_error;
	}

	assert(decl->type_ref->spec->kind == type_func_sig);

	ast_func_params_t* params = ast_func_decl_params(decl);
	if(params->ellipse_param && params->param_count == 0)
	{
		_report_err(decl->tokens.start, ERR_SYNTAX,
			"use of ellipse parameter requires at least one other parameter",
			name);
		return proc_decl_error;
	}

	if (!resolve_function_decl_types(decl))
		return proc_decl_error;

	if (exist)
	{
		if (_is_fn_definition(exist) && _is_fn_definition(decl))
		{
			//multiple definition
			_report_err(decl->tokens.start, ERR_DUP_SYMBOL,
				"redefinition of function '%s'", name);
			return proc_decl_error;
		}

		if (!sema_is_same_func_sig(exist->type_ref->spec->data.func_sig_spec, decl->type_ref->spec->data.func_sig_spec))
		{
			_report_err(decl->tokens.start, ERR_INVALID_PARAMS,
				"differing function signature in definition of '%s'", name);
			return proc_decl_error;
		}

		if (!_is_fn_definition(exist) && _is_fn_definition(decl))
		{
			//update definition
			//exist->data.func
			idm_update_decl(_id_map, decl);
		}
	}
	else
	{
		idm_add_decl(_id_map, decl);
	}

	return _is_fn_definition(decl) ? proc_decl_new_def : proc_decl_ignore;
}

/*
Multiple declarations are allowed
Only a single definition is permitted
Type cannot change
*/
proc_decl_result process_global_variable_declaration(ast_declaration_t* decl)
{
	ast_declaration_t* exist = idm_find_decl(_id_map, decl->name);
	if (exist && exist->kind == decl_func)
	{
		_report_err(decl->tokens.start, ERR_DUP_SYMBOL,
			"global var declaration of '%s' shadows function at",
			decl->name);
		return proc_decl_error;
	}

	if (!sema_resolve_type_ref(decl->type_ref))
	{		
		return proc_decl_error;
	}

	ast_type_spec_t* type = decl->type_ref->spec;
	
	if (exist && exist->kind == decl_var)
	{		
		//multiple declarations are allowed		
		if (!type || !sema_is_same_type(exist->type_ref->spec, type))
		{
			//different types
			_report_err(decl->tokens.start, ERR_DUP_SYMBOL,
				"incompatible redeclaration of global var '%s'",
				decl->name);
			return proc_decl_error;
		}

		if (exist->data.var.init_expr && decl->data.var.init_expr)
		{
			//multiple definitions
			_report_err(decl->tokens.start, ERR_DUP_SYMBOL,
				"redefinition of global var '%s'",
				decl->name);
			return proc_decl_error;
		}

		if (!exist->data.var.init_expr && decl->data.var.init_expr)
		{
			//update definition
			exist->data.var.init_expr = decl->data.var.init_expr;
			decl->data.var.init_expr = NULL;
			process_expression(exist->data.var.init_expr);
		}
		return proc_decl_ignore;
	}

	if (type->size == 0)
	{
		_report_err(decl->tokens.start, ERR_TYPE_INCOMPLETE,
			"global var '%s' uses incomplete type '%s'",
			decl->name, ast_type_name(decl->type_ref->spec));
		return proc_decl_error;
	}

	if (decl->data.var.init_expr)
	{
		if (!process_expression(decl->data.var.init_expr))
			return false;

		ast_type_spec_t* var_type = decl->type_ref->spec;

		if (decl->data.var.init_expr->kind == expr_int_literal)
		{
			if (!int_val_will_fit(&decl->data.var.init_expr->data.int_literal.val, var_type))
			{
				_report_err(decl->data.var.init_expr->tokens.start,
					ERR_INCOMPATIBLE_TYPE,
					"assignment to incompatible type. expected %s",
					ast_type_name(var_type));
				return proc_decl_error;
			}
		}
		else if (decl->data.var.init_expr->kind != expr_str_literal)
		{
			return _report_err(decl->tokens.start, ERR_INITIALISER_NOT_CONST,
				"global var '%s' initialised with non-const expression", decl->name);
		}
	}

	idm_add_decl(_id_map, decl);
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

valid_trans_unit_t* sem_analyse(ast_trans_unit_t* ast)
{
	types_init();
	_id_map = idm_create();

	_cur_func_ctx.decl = NULL;
	_cur_func_ctx.labels = NULL;

	valid_trans_unit_t* tl = (valid_trans_unit_t*)malloc(sizeof(valid_trans_unit_t));
	memset(tl, 0, sizeof(valid_trans_unit_t));
	tl->ast = ast;
	tl->string_literals = _id_map->string_literals;
	ast_declaration_t* decl = ast->decls.first;
	while (decl)
	{
		ast_declaration_t* next = decl->next;
		if (decl->kind == decl_var)
		{
			proc_decl_result result = process_global_variable_declaration(decl);

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
					if (!process_function_definition(decl))
					{
						return NULL;
					}
					
				}
			}
		}
		else if (decl->kind == decl_type)
		{
			process_type_decl(decl);
		}
		decl = next;
	}
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
