#include "sema.h"
#include "sema_internal.h"
#include "sema_types.h"
#include "diag.h"
#include "var_set.h"
#include "id_map.h"
#include "abi.h"

#include "std_types.h"

#include <libj/include/hash_table.h>

#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

static identfier_map_t* _id_map;
static sema_observer_t _observer;

identfier_map_t* sema_id_map()
{
	return _id_map; 
}

static func_context_t _cur_func_ctx;

func_context_t* sema_get_cur_fn_ctx()
{
	return &_cur_func_ctx;
}

proc_decl_result sema_process_function_decl(ast_declaration_t* decl);
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

static bool process_expression(ast_expression_t* expr)
{
	if (!expr)
		return true;

	expr_result_t result = sema_process_expression(expr);

	return !result.failure;
}

static bool _is_member_name_unique(ast_user_type_spec_t* user_type_spec, ast_struct_member_t* unique)
{
	ast_struct_member_t* member = user_type_spec->data.struct_members;
	while (member)
	{
		if (member != unique && strcmp(member->decl->name, unique->decl->name) == 0)
			return false;
		member = member->next;
	}
	return true;
}

static bool _process_struct_members(ast_user_type_spec_t* user_type_spec)
{
	assert(user_type_spec->kind != user_type_enum);
	ast_struct_member_t* member = user_type_spec->data.struct_members;
	while (member)
	{
		if (strlen(member->decl->name) > 0 && !_is_member_name_unique(user_type_spec, member))
		{
			return _report_err(member->decl->tokens.start, ERR_DUP_SYMBOL,
				"duplicate %s member %s",
				ast_user_type_kind_name(user_type_spec->kind),
				member->decl->name);
		}

		if (!sema_resolve_type_ref(member->decl->type_ref) || member->decl->type_ref->spec->size == 0)
		{
			return _report_err(member->decl->tokens.start, ERR_TYPE_INCOMPLETE,
				"%s member %s is of incomplete type",
				ast_user_type_kind_name(user_type_spec->kind),
				member->decl->name);
		}

		if (ast_is_bit_field_member(member))
		{
			if (ast_type_is_array(member->decl->type_ref->spec))
			{
				return _report_err(member->decl->data.var.bit_sz->tokens.start, ERR_UNSUPPORTED,
					"bit field cannt be declared as an array");
			}

			if (!ast_type_is_int(member->decl->type_ref->spec))
			{
				return _report_err(member->decl->data.var.bit_sz->tokens.start, ERR_UNSUPPORTED,
					"type not valid for bit field");
			}

			if (!sema_is_const_int_expr(member->decl->data.var.bit_sz))
			{
				return _report_err(member->decl->data.var.bit_sz->tokens.start, ERR_INITIALISER_NOT_CONST,
					"bit field size must be a constant integer expression");
			}

			int_val_t val = sema_fold_const_int_expr(member->decl->data.var.bit_sz);

			if (val.v.uint64 > 32)
			{
				return _report_err(member->decl->data.var.bit_sz->tokens.start, ERR_UNSUPPORTED,
					"bit field size exceeds 32 bits");
			}

			if (val.v.int64 < 0)
			{
				return _report_err(member->decl->data.var.bit_sz->tokens.start, ERR_UNSUPPORTED,
					"bit field size must be positive");
			}

			if (val.v.uint64 == 0 && strlen(member->decl->name) > 0)
			{
				return _report_err(member->decl->data.var.bit_sz->tokens.start, ERR_UNSUPPORTED,
					"field with bit field size 0 must be anonymous");
			}

			member->sema.bit_field.size = (size_t)val.v.uint64;
			member->sema.bit_field.offset = 0;
		}

		member = member->next;
	}
	return true;
}

static ast_declaration_t* _create_enum_item_decl(ast_enum_member_t* member, int_val_t default_val)
{
	ast_expression_t* value;
	if (member->value)
	{
		if (!sema_is_const_int_expr(member->value))
		{
			_report_err(member->value->tokens.start, ERR_INITIALISER_NOT_CONST,
				"enum member value must be a constant integer expression");
			return NULL;
		}
		sema_fold_const_int_expr(member->value);
		value = member->value;
	}
	else
	{
		value = (ast_expression_t*)malloc(sizeof(ast_expression_t));
		memset(value, 0, sizeof(ast_expression_t));
		value->tokens = member->tokens; //?
		value->kind = expr_int_literal;
		value->data.int_literal.val = default_val;
		
	}
	//create a declaration representing the enumerator
	ast_declaration_t* decl = (ast_declaration_t*)malloc(sizeof(ast_declaration_t));
	memset(decl, 0, sizeof(ast_declaration_t));
	decl->tokens = member->tokens;
	decl->kind = decl_var;
	strncpy(decl->name, member->name, MAX_LITERAL_NAME);
	//type is int32
	decl->type_ref = (ast_type_ref_t*)malloc(sizeof(ast_type_ref_t));
	decl->type_ref->tokens = member->tokens; //?
	decl->type_ref->spec = int32_type_spec;
	decl->type_ref->flags = TF_QUAL_CONST;
	//const int init expression
	decl->data.var.init_expr = value;

	return decl;
}

/*
Create asl_declaration_t objects for each item in the enum
*/
static bool _process_enum_constants(ast_user_type_spec_t* user_type_spec)
{
	assert(user_type_spec->kind == user_type_enum);
	ast_enum_member_t* member = user_type_spec->data.enum_members;
	int_val_t next_val = int_val_zero();
	
	while (member)
	{
		ast_declaration_t* exist = idm_find_decl(_id_map, member->name);
		if (exist)
		{
			_report_err(member->tokens.start, ERR_DUP_SYMBOL,
				"duplicate enumerator %s, previously defined as %s", member->name, ast_decl_kind_name(exist->kind));
			return false;
		}

		ast_declaration_t* decl = _create_enum_item_decl(member, next_val);

		if (!decl)
			return false;

		idm_add_decl(_id_map, decl);

		next_val = int_val_inc(decl->data.var.init_expr->data.int_literal.val);
		member = member->next;
	}
	return true;
}

static bool _user_type_is_definition(ast_user_type_spec_t* type)
{
	if (type->kind == user_type_enum)
		return type->data.enum_members != NULL;
	return type->data.struct_members != NULL;
}

static ast_type_spec_t* _process_user_type(ast_type_spec_t* spec)
{
	//add early as we may have members of our own type
	idm_add_tag(_id_map, spec); //todo - what about anonymous structs?

	bool valid;
	if (spec->data.user_type_spec->kind == user_type_enum)
		valid = _process_enum_constants(spec->data.user_type_spec);
	else
		valid = _process_struct_members(spec->data.user_type_spec);

	if (!valid)
		return NULL;

	//dont calc size for declarations
	if(spec->data.user_type_spec->data.struct_members)
		spec->size = abi_calc_user_type_layout(spec);
	
	return spec;
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

		return decl->type_ref->spec;
	}

	if (spec->kind == type_ptr)
	{
		spec->data.ptr_type = sema_resolve_type(spec->data.ptr_type, start);

		if (!spec->data.ptr_type)
		{
			_report_err(start, ERR_UNKNOWN_TYPE,
				"pointer to unknown type");
			return NULL;
		}
		return spec;
	}

	if (spec->kind == type_array )// && spec->data.array_spec->size_expr->kind != expr_null && spec->data.array_spec->sema.array_sz == 0)
	{
		if (spec->size == 0 && spec->data.array_spec->size_expr->kind != expr_null)
		{
			ast_array_spec_t* array_spec = spec->data.array_spec;

			array_spec->element_type = sema_resolve_type(array_spec->element_type, start);
			if (!array_spec->element_type || array_spec->element_type->size == 0)
			{
				_report_err(array_spec->size_expr->tokens.start, ERR_INITIALISER_NOT_CONST,
					"array element size unknown");
				return NULL;
			}

			if (!sema_is_const_int_expr(array_spec->size_expr))
			{
				_report_err(array_spec->size_expr->tokens.start, ERR_INITIALISER_NOT_CONST,
					"array size must be a constant integer expression");
				return NULL;
			}
			int_val_t expr_val = sema_fold_const_int_expr(array_spec->size_expr);

			if (expr_val.is_signed && expr_val.v.int64 <= 0)
			{
				_report_err(array_spec->size_expr->tokens.start, ERR_INVALID_INIT,
					"array size must be a positive integer expression");
				return NULL;
			}
			array_spec->sema.array_sz = (size_t)expr_val.v.uint64;
			spec->size = array_spec->sema.array_sz * array_spec->element_type->size;
		}
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
			return _process_user_type(spec);
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
					ast_user_type_kind_name(exist->data.user_type_spec->kind),
					spec->data.user_type_spec->name,
					ast_user_type_kind_name(spec->data.user_type_spec->kind));
				return NULL;
			}

			if (_user_type_is_definition(exist->data.user_type_spec))
			{
				//multiple definitions
				_report_err(loc, ERR_DUP_TYPE_DEF,
					"redefinition of %s '%s'",
					ast_user_type_kind_name(spec->data.user_type_spec->kind),
					spec->data.user_type_spec->name);
				return NULL;
			}
			//update the existing declaration
			if (spec->data.user_type_spec->kind == user_type_enum)
			{
				exist->data.user_type_spec->data.enum_members = spec->data.user_type_spec->data.enum_members;
				spec->data.user_type_spec->data.enum_members = NULL;
				_process_enum_constants(exist->data.user_type_spec);
			}
			else
			{
				//update definition
				exist->data.user_type_spec->data.struct_members = spec->data.user_type_spec->data.struct_members;
				spec->data.user_type_spec->data.struct_members = NULL;
				_process_struct_members(exist->data.user_type_spec);
			}
			exist->size = abi_calc_user_type_layout(exist);
			ast_destroy_type_spec(spec);
			spec = exist;
		}
		else
		{
			spec = _process_user_type(spec);
		}

		if (_observer.user_type_def_cb)
			_observer.user_type_def_cb(spec);
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
					ast_user_type_kind_name(exist->data.user_type_spec->kind),
					spec->data.user_type_spec->name,
					ast_user_type_kind_name(spec->data.user_type_spec->kind));
				return NULL;
			}
			ast_destroy_type_spec(spec);
			spec = exist;
		}
		else
		{
			spec = _process_user_type(spec);
		}
	}

	//treat enums as int32_t
	if (spec && ast_type_is_enum(spec))
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

	if (lhs->kind == type_array)
	{
		return sema_is_same_type(lhs->data.array_spec->element_type, rhs->data.array_spec->element_type) && 
				lhs->data.array_spec->sema.array_sz == rhs->data.array_spec->sema.array_sz;
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

	if (ast_type_is_ptr(target) && ast_type_is_array(type))
	{
		//any array can be converted to void*
		if (target->data.ptr_type->kind == type_void)
			return true;

		//converting an array to a pointer
		return sema_can_convert_type(target->data.ptr_type, type->data.array_spec->element_type);
	}

	//integer promotion
	if (ast_type_is_int(target) && ast_type_is_int(type) && target->size >= type->size)
	{
		return true;
	}

	//are they pointing at convertable types?
	if (target->kind == type_ptr && type->kind == type_ptr)
	{
		//any pointer can be converted to void*
		if (target->data.ptr_type->kind == type_void)
			return true;

		return sema_can_convert_type(target->data.ptr_type, type->data.ptr_type);
	}

	//todo - function pointers
	if (target->kind == type_func_sig && type->kind == type_func_sig)
	{

	}

	return false;
}


bool process_declaration(ast_declaration_t* decl)
{
	if (!decl)
		return true;
	switch (decl->kind)
	{
	case decl_var:
		return sema_process_variable_declaration(decl);
	case decl_func:
		return sema_process_function_decl(decl) != proc_decl_error;
	case decl_type:
		return sema_process_type_decl(decl);
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
	phs_insert(sema_get_cur_fn_ctx()->goto_smnts, smnt);
	return true;
}

bool process_label_statement(ast_statement_t* smnt)
{
	if(sht_contains(sema_get_cur_fn_ctx()->labels, smnt->data.label_smnt.label))
	{
		return _report_err(smnt->tokens.start, ERR_DUP_LABEL,
			"dupliate label '%s' in function '%s'",
			smnt->data.label_smnt.label, sema_get_cur_fn_ctx()->decl->name);
	}
	sht_insert(sema_get_cur_fn_ctx()->labels, smnt->data.label_smnt.label, 0);

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
	assert(sema_get_cur_fn_ctx()->decl);

	expr_result_t result = sema_process_expression(smnt->data.expr);
	if (result.failure)
		return false;
	
	if (smnt->data.expr && smnt->data.expr->kind != expr_null)
	{
		if (!sema_can_convert_type(ast_func_decl_return_type(sema_get_cur_fn_ctx()->decl), result.result_type))
		{
			sema_report_type_conversion_error(smnt->data.expr, ast_func_decl_return_type(sema_get_cur_fn_ctx()->decl), result.result_type, "return statement");
			return false;
		}
	}
	else if (sema_get_cur_fn_ctx()->decl->type_ref->spec->kind != type_void)
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

	sema_get_cur_fn_ctx()->decl = decl;
	sema_get_cur_fn_ctx()->labels = sht_create(64);
	sema_get_cur_fn_ctx()->goto_smnts = phs_create(64);
	
	bool ret = process_block_list(decl->data.func.blocks);

	//check that any goto statements reference valid labels
	phs_iterator_t it = phs_begin(sema_get_cur_fn_ctx()->goto_smnts);
	while (!phs_end(sema_get_cur_fn_ctx()->goto_smnts, &it))
	{
		ast_statement_t* goto_smnt = (ast_statement_t*)it.val;

		if (!sht_contains(sema_get_cur_fn_ctx()->labels, goto_smnt->data.goto_smnt.label))
		{
			return _report_err(goto_smnt->tokens.start, ERR_UNKNOWN_LABEL,
				"goto statement references unknown label '%s'",
				goto_smnt->data.goto_smnt.label);

		}
		phs_next(sema_get_cur_fn_ctx()->goto_smnts, &it);
	}

	ht_destroy(sema_get_cur_fn_ctx()->goto_smnts);
	sema_get_cur_fn_ctx()->goto_smnts = NULL;
	ht_destroy(sema_get_cur_fn_ctx()->labels);
	sema_get_cur_fn_ctx()->labels = NULL;
	sema_get_cur_fn_ctx()->decl = NULL;

	idm_leave_function(_id_map);
	return ret;
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

void sema_init(sema_observer_t observer)
{
	types_init();
	_id_map = idm_create();
	_observer = observer;
}

valid_trans_unit_t* sema_analyse(ast_trans_unit_t* ast)
{
	sema_get_cur_fn_ctx()->decl = NULL;
	sema_get_cur_fn_ctx()->labels = NULL;

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
			proc_decl_result result = sema_process_global_variable_declaration(decl);

			if (result == proc_decl_error)
				return NULL;
			if (result == proc_decl_new_def)
			{
				_add_var_decl(tl, decl);
			}
		}
		else if (decl->kind == decl_func)
		{
			proc_decl_result result = sema_process_function_decl(decl);
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
			sema_process_type_decl(decl);
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
