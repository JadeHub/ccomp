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
bool process_statement(ast_statement_t* smnt);
static bool sema_resolve_type_ref(ast_type_ref_t* ref);

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
		sema_resolve_type_ref(member->type_ref);
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
			if (!sema_is_int_constant_expression(member->value))
			{
				//err
				return false;
			}
			next_val = sema_eval_constant_expr(member->value);			
		}

		//idm_find_decl

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
		result += member->type_ref->spec->size;
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

static bool sema_resolve_type_ref(ast_type_ref_t* ref)
{
	assert(ref->spec);
	ast_type_spec_t* spec = sema_resolve_type(ref->spec);
	if (spec)
	{
		ref->spec = spec;
		return true;
	}
	return false;
}

ast_type_spec_t* sema_resolve_type(ast_type_spec_t* typeref)
{
	if (typeref->kind == type_ptr)
	{
		typeref->data.ptr_type = sema_resolve_type(typeref->data.ptr_type);
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
	token_t* loc = typeref->data.user_type_spec->tokens.start;
	if (_user_type_is_definition(typeref->data.user_type_spec))
	{
		ast_type_spec_t* exist = idm_find_block_tag(_id_map, typeref->data.user_type_spec->name);
		if (typeref == exist)
			return exist;

		if (exist)
		{
			if (exist->data.user_type_spec->kind != typeref->data.user_type_spec->kind)
			{
				_report_err(loc, ERR_DUP_TYPE_DEF,
					"redefinition of %s '%s' changes type to %s",
					user_type_kind_name(exist->data.user_type_spec->kind),
					typeref->data.user_type_spec->name,
					user_type_kind_name(typeref->data.user_type_spec->kind));
				return NULL;
			}

			if (_user_type_is_definition(exist->data.user_type_spec))
			{
				//multiple definitions
				_report_err(loc, ERR_DUP_TYPE_DEF,
					"redefinition of %s '%s'",
					user_type_kind_name(typeref->data.user_type_spec->kind),
					typeref->data.user_type_spec->name);
				return NULL;
			}
			//update the existing declaration
			if (typeref->data.user_type_spec->kind == user_type_enum)
			{
				exist->data.user_type_spec->data.enum_members = typeref->data.user_type_spec->data.enum_members;
				typeref->data.user_type_spec->data.enum_members = NULL;
				_register_enum_constants(exist->data.user_type_spec);
			}
			else
			{
				//update definition
				exist->data.user_type_spec->data.struct_members = typeref->data.user_type_spec->data.struct_members;
				typeref->data.user_type_spec->data.struct_members = NULL;
				_resolve_struct_member_types(exist->data.user_type_spec);
			}
			exist->size = _calc_user_type_size(exist);
			ast_destroy_type_spec(typeref);
			typeref = exist;
		}
		else
		{
			_add_user_type(typeref);
		}
	}
	else
	{
		//declaration
		ast_type_spec_t* exist = idm_find_tag(_id_map, typeref->data.user_type_spec->name);
		if (exist)
		{
			if (exist->data.user_type_spec->kind != typeref->data.user_type_spec->kind)
			{
				_report_err(loc, ERR_DUP_TYPE_DEF,
					"redefinition of %s '%s' changes type to %s",
					user_type_kind_name(exist->data.user_type_spec->kind),
					typeref->data.user_type_spec->name,
					user_type_kind_name(typeref->data.user_type_spec->kind));
				return NULL;
			}
			ast_destroy_type_spec(typeref);
			typeref = exist;
		}
		else
		{
			_add_user_type(typeref);
		}
	}

	//treat enums as int32_t
	if (ast_type_is_enum(typeref))
		return int32_type_spec;

	return typeref;
}

bool sema_can_convert_type(ast_type_spec_t* target, ast_type_spec_t* type)
{
	if (!type)
		return false;

	if (target == type)
		return true;

	//integer promotion
	if (ast_type_is_int(target) && ast_type_is_int(type) && target->size > type->size)
	{
		return true;
	}
		

	if (target->kind == type_ptr && type->kind == type_ptr)
		return sema_can_convert_type(target->data.ptr_type, type->data.ptr_type);

	return false;
}

void f()
{

}

bool process_variable_declaration(ast_declaration_t* decl)
{
	ast_var_decl_t* var = &decl->data.var;

	ast_declaration_t* existing = idm_find_block_decl(_id_map, var->name, decl_var);
	if (existing)
	{
		return _report_err(decl->tokens.start, ERR_DUP_VAR,
			"var %s already declared at",
			ast_declaration_name(decl));
	}

	if(!sema_resolve_type_ref(var->type_ref) || var->type_ref->spec->size == 0)
	{
		return _report_err(decl->tokens.start, ERR_TYPE_INCOMPLETE,
			"var %s is of incomplete type",
			ast_declaration_name(decl));
	}

	if (decl->data.var.expr)
	{
		expr_result_t result = sema_process_expression(decl->data.var.expr);
		if (result.failure)
			return false;

		ast_type_spec_t* init_type = result.result_type;

		if (!sema_can_convert_type(var->type_ref->spec, init_type))
		{
			return _report_err(decl->data.var.expr->tokens.start,
				ERR_INCOMPATIBLE_TYPE,
				"assignment to incompatible type. expected %s",
				ast_type_name(var->type_ref->spec));
		}
	}

	idm_add_id(_id_map, decl);
	_cur_func_ctx.decl->required_stack_size += var->type_ref->spec->size;

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
		return sema_resolve_type(decl->data.type);		
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
		return process_declaration(block->data.decl);
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

	ast_switch_case_data_t* case_data = smnt->data.switch_smnt.cases;
	while (case_data)
	{
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
		if (!sema_can_convert_type(_cur_func_ctx.decl->return_type_ref->spec, result.result_type))
		{
			return _report_err(smnt->tokens.start, ERR_INVALID_RETURN,
				"return value type does not match function declaration");
		}
	}
	else if (_cur_func_ctx.decl->return_type_ref->spec->kind != type_void)
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
	ast_function_decl_t* func = &decl->data.func;

	//return type
	if (!sema_resolve_type_ref(func->return_type_ref))
		return false;
	
	ast_type_spec_t* ret_type = func->return_type_ref->spec;

	if (_is_fn_definition(decl) && 
		ret_type->kind != type_void && 
		ret_type->size == 0)
	{
		return _report_err(decl->tokens.start, ERR_TYPE_INCOMPLETE,
			"function '%s' returns incomplete type '%s'",
			func->name, ret_type->data.user_type_spec->name);
	}
	
	//parameter types
	ast_func_param_decl_t* param = func->first_param;
	while (param)
	{
		if (param->decl->data.var.type_ref->spec->kind == type_void)
		{
			return _report_err(param->decl->tokens.start, ERR_INVALID_PARAMS,
				"function param '%s' of void type",
				param->decl->data.var.name);
		}

		if (!sema_resolve_type_ref(param->decl->data.var.type_ref))
			return false;

		ast_type_spec_t* param_type = param->decl->data.var.type_ref->spec;
		
		if (_is_fn_definition(decl) && 
			param_type->kind != type_void &&
			param_type->size == 0)
		{
			return _report_err(param->decl->tokens.start, ERR_TYPE_INCOMPLETE,
				"function param '%s' returns incomplete type '%s'",
				param->decl->data.var.name, param_type->data.user_type_spec->name);
		}
		
		param = param->next;
	}
	return true;
}

static bool _compare_func_decls(ast_declaration_t* exist, ast_declaration_t* func)
{
	if (exist->data.func.return_type_ref->spec != func->data.func.return_type_ref->spec)
	{
		return _report_err(func->tokens.start, ERR_INVALID_PARAMS,
			"differing return type in declaration of function '%s'. Expected '%s'",
			ast_declaration_name(func), ast_type_ref_name(exist->data.func.return_type_ref));
	}

	if (exist->data.func.param_count != func->data.func.param_count)
	{
		return _report_err(func->tokens.start, ERR_INVALID_PARAMS,
			"incorrect number of params in declaration of function '%s'. Expected %d",
			ast_declaration_name(func), exist->data.func.param_count);
	}

	ast_func_param_decl_t* p_exist = exist->data.func.first_param;
	ast_func_param_decl_t* p_func = func->data.func.first_param;

	int p_count = 1;
	while (p_exist && p_func)
	{
		if (p_exist->decl->data.var.type_ref->spec != sema_resolve_type(p_func->decl->data.var.type_ref->spec))
		{
			return _report_err(func->tokens.start, ERR_INVALID_PARAMS,
				"conflicting type in param %d of declaration of function '%s'. Expected '%s'",
				p_count, ast_declaration_name(func), ast_type_ref_name(p_exist->decl->data.var.type_ref));
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

static bool _process_global_init_expression(ast_declaration_t* decl)
{
	process_expression(decl->data.var.expr);
	if (!sema_is_int_constant_expression(decl->data.var.expr))
	{
		return _report_err(decl->tokens.start, ERR_INITIALISER_NOT_CONST,
			"global var '%s' initialised with non-const expression", decl->data.var.name);
	}

	if (decl->data.var.expr->kind != expr_int_literal)
	{
		int_val_t val = sema_eval_constant_expr(decl->data.var.expr);

		ast_expression_t* expr = (ast_expression_t*)malloc(sizeof(ast_expression_t));
		memset(expr, 0, sizeof(ast_expression_t));
		expr->kind = expr_int_literal;
		expr->data.int_literal.val = val;
		process_expression(expr);
		ast_destroy_expression(decl->data.var.expr);
		decl->data.var.expr = expr;
	}
	return true;
}

/*
Multiple declarations are allowed
Only a single definition is permitted
Type cannot change
*/
proc_decl_result process_global_variable_declaration(ast_declaration_t* decl)
{
	ast_var_decl_t* var = &decl->data.var;

	ast_declaration_t* fn = idm_find_decl(_id_map, var->name, decl_func);
	if (fn)
	{
		_report_err(decl->tokens.start, ERR_DUP_SYMBOL,
			"global var declaration of '%s' shadows function at",
			var->name);
		return proc_decl_error;
	}

	if (!sema_resolve_type_ref(decl->data.var.type_ref))
	{		
	//	return _report_err(decl->tokens.start, ERR_TYPE_INCOMPLETE,
	//		"var %s is of incomplete type",	var->name);
		return proc_decl_error;
	}

	ast_type_spec_t* type = var->type_ref->spec;
	
	ast_declaration_t* exist = idm_find_decl(_id_map, var->name, decl_var);
	if (exist)
	{		
		//multiple declarations are allowed
		if (!type || exist->data.var.type_ref->spec != type)
		{
			//different types
			_report_err(decl->tokens.start, ERR_DUP_SYMBOL,
				"incompatible redeclaration of global var '%s'",
				var->name);
			return proc_decl_error;
		}

		if (exist->data.var.expr && decl->data.var.expr)
		{
			//multiple definitions
			_report_err(decl->tokens.start, ERR_DUP_SYMBOL,
				"redefinition of global var '%s'",
				var->name);
			return proc_decl_error;
		}

		if (!exist->data.var.expr && decl->data.var.expr)
		{
			//update definition
			exist->data.var.expr = decl->data.var.expr;
			decl->data.var.expr = NULL;
			process_expression(exist->data.var.expr);
		}
		return proc_decl_ignore;
	}

	if (type->size == 0)
	{
		_report_err(decl->tokens.start, ERR_TYPE_INCOMPLETE,
			"global var '%s' uses incomplete type '%s'",
			var->name, ast_type_ref_name(decl->data.var.type_ref));
		return proc_decl_error;
	}

	if (decl->data.var.expr)
	{
		if (!process_expression(decl->data.var.expr))
			return false;

		ast_type_spec_t* var_type = decl->data.var.type_ref->spec;

		if (decl->data.var.expr->kind == expr_int_literal)
		{
			if (!int_val_will_fit(&decl->data.var.expr->data.int_literal.val, var_type))
			{
				_report_err(decl->data.var.expr->tokens.start,
					ERR_INCOMPATIBLE_TYPE,
					"assignment to incompatible type. expected %s",
					ast_type_name(var_type));
				return proc_decl_error;
			}
		}
		else if (decl->data.var.expr->kind == expr_str_literal)
		{

		}
		else
		{
			return _report_err(decl->tokens.start, ERR_INITIALISER_NOT_CONST,
				"global var '%s' initialised with non-const expression", var->name);
		}

		//_process_global_init_expression(decl);

		

		
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
	tl->string_literals = _id_map->string_literals;
	ast_declaration_t* decl = ast->decls;
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
					if (!process_function_definition(&decl->data.func))
					{
						return NULL;
					}
					
				}
			}
		}
		else if (decl->kind == decl_type)
		{
			ast_type_spec_t* type = sema_resolve_type(decl->data.type);

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