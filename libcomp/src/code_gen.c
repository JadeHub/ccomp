#include "code_gen.h"
#include "source.h"

#include "id_map.h"
#include "std_types.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

void gen_statement(ast_statement_t* smnt);
void gen_block_item(ast_block_item_t* bi);

static write_asm_cb _asm_cb;
static void* _asm_cb_data;
static bool _annotation = false;
static uint32_t gen_annotate_depth = 0;

static var_set_t* _var_set;
static bool _returned = false;
static ast_declaration_t* _cur_fun = NULL;
static valid_trans_unit_t* _cur_tl = NULL;

static const char* _cur_break_label = NULL;
static const char* _cur_cont_label = NULL;

var_set_t* gen_var_set()
{
	return _var_set;
}

void gen_asm(const char* format, ...)
{
	char buff[256];

	va_list args;
	va_start(args, format);
	vsnprintf(buff, 256, format, args);
	va_end(args);

	if (_annotation)
	{
		int tabs = gen_annotate_depth;
		while ((--tabs) > 0)
		{
			_asm_cb("\t", false, _asm_cb_data);
		}
	}

	_asm_cb(buff, true, _asm_cb_data);
}

void gen_annotate(const char* format, ...)
{
	if (_annotation)
	{
		char buff[256];

		va_list args;
		va_start(args, format);
		vsnprintf(buff, 256, format, args);
		va_end(args);

		gen_asm("#%s", buff);
	}
}

void gen_annotate_start(const char* format, ...)
{
	gen_annotate_depth++;
	if (_annotation)
	{
		char buff[256];

		va_list args;
		va_start(args, format);
		vsnprintf(buff, 256, format, args);
		va_end(args);

		//newline before tabbed annotation start message
		_asm_cb("", true, _asm_cb_data);
		gen_asm("#%s", buff);
	}
}

void gen_annotate_end()
{
	gen_annotate_depth--;
}

//todo - replace with sema version
void gen_make_label_name(char* name)
{
	static uint32_t next_label = 0;
	sprintf(name, "_label%d", ++next_label);
}

static bool _is_unsigned_int_type(ast_type_spec_t* type)
{
	return type->kind == type_uint8 ||
		type->kind == type_uint16 ||
		type->kind == type_uint32;
}

static const char* _sized_mov(ast_type_spec_t* type)
{
	if (type->size == 4)
		return "movl";

	if (type->size == 2)
		return "movw";

	if (type->size == 1)
		return "movb";

	assert(false);
	return "";
}

static void _gen_mov_a_to_stack(size_t sz, int offset)
{
	if (sz == 1)
	{
		gen_asm("movb %%al, %d(%%ebp)", offset);
	}
	else if (sz == 2)
	{
		gen_asm("movw %%ax, %d(%%ebp)", offset);
	}
	else if (sz == 4)
	{
		gen_asm("movl %%eax, %d(%%ebp)", offset);
	}
	else
	{
		assert(false);
	}
}

static const char* _promoting_mov_instr(ast_type_spec_t* type)
{
	if (type->size == 4)
		return "movl";

	if (type->size == 2)
		return _is_unsigned_int_type(type) ? "movzwl" : "movswl";

	if (type->size == 1)
		return _is_unsigned_int_type(type) ? "movzbl" : "movsbl";

	assert(false);
	return "";
}

ast_type_spec_t* _get_func_sig_ret_type(ast_type_ref_t* func_sig)
{
	assert(func_sig->spec->kind == type_func_sig);
	return func_sig->spec->data.func_sig_spec->ret_type;
}

void gen_compound_init(int32_t bsp_offset, ast_type_spec_t* decl_type, ast_expr_compound_init_t* init);

void gen_struct_union_init(int32_t bsp_offset, ast_user_type_spec_t* decl_type, ast_expr_compound_init_t* init)
{
	gen_annotate_start("struct init expression");
	ast_compound_init_item_t* init_item = init->item_list;
	while (init_item)
	{
		ast_struct_member_t* member = init_item->sema.member;
		gen_annotate("init member %s", member->decl->name);

		if (init_item->expr->kind == expr_compound_init)
		{
			gen_compound_init(bsp_offset + (int32_t)member->sema.offset,
				member->decl->type_ref->spec, &init_item->expr->data.compound_init);
		}
		else
		{
			gen_expression(init_item->expr);
			_gen_mov_a_to_stack(member->decl->type_ref->spec->size, bsp_offset + (int32_t)member->sema.offset);
		}

		if (decl_type->kind == user_type_union)
			break; //only init the first member for unions

		member = member->next;
		init_item = init_item->next;
	}
	gen_annotate_end();
}

void gen_array_compound_init(int32_t bsp_offset, ast_type_spec_t* elem_type, ast_expr_compound_init_t* init)
{
	gen_annotate_start("struct init expression");
	ast_compound_init_item_t* init_item = init->item_list;
	while (init_item)
	{
		if (init_item->expr->kind == expr_compound_init)
		{
			gen_compound_init(bsp_offset,
				elem_type, &init_item->expr->data.compound_init);
		}
		else
		{
			gen_expression(init_item->expr);
			_gen_mov_a_to_stack(elem_type->size, bsp_offset);
		}
		bsp_offset += (int32_t)elem_type->size;

		init_item = init_item->next;
	}
	gen_annotate_end();
}

void gen_compound_init(int32_t bsp_offset, ast_type_spec_t* decl_type, ast_expr_compound_init_t* init)
{
	if (ast_type_is_struct_union(decl_type))
	{
		gen_struct_union_init(bsp_offset, decl_type->data.user_type_spec, init);
	}
	else if (ast_type_is_array(decl_type))
	{
		gen_array_compound_init(bsp_offset, decl_type->data.array_spec->element_type, init);
	}
}

void gen_var_decl(ast_declaration_t* decl)
{
	var_data_t* var = var_decl_stack_var(_var_set, decl);
	assert(var);
	gen_annotate_start("local variable '%s' at stack %d", decl->name, var->bsp_offset);
	if (decl->data.var.init_expr)
	{
		if (decl->data.var.init_expr->kind == expr_compound_init)
		{
			gen_compound_init(var->bsp_offset, decl->type_ref->spec, &decl->data.var.init_expr->data.compound_init);
		}
		else
		{
			ast_expression_t* expr = decl->data.var.init_expr;

			gen_annotate("init expression");
			gen_expression(expr);

			gen_annotate("assignment");
			//if the target is a user defined type we assume eax and edx are pointers
			if (expr->sema.result.type->kind == type_user)
			{
				int32_t dest_off = var->bsp_offset;
				uint32_t offset = 0;
				while (offset < expr->sema.result.type->size)
				{
					gen_asm("movl %d(%%eax), %%ecx", offset);
					gen_asm("movl %%ecx, %d(%%ebp)", dest_off);
					offset += 4;
					dest_off += 4;
				}
			}
			else
			{
				_gen_mov_a_to_stack(decl->type_ref->spec->size, var->bsp_offset);
			}
		}
	}
	else
	{
		gen_annotate("init with zero");
		for (int i = 0; i < decl->type_ref->spec->size / 4; i++)
		{
			gen_asm("movl $0, %d(%%ebp)", var->bsp_offset + i * 4);
		}
	}
	gen_annotate_end();
}

void gen_scope_block_enter()
{
	var_enter_block(_var_set);
}

void gen_scope_block_leave()
{
	var_leave_block(_var_set);
}

void gen_case_statement(ast_statement_t* smnt)
{
	ast_case_smnt_data_t* case_data = &smnt->data.case_smnt;
	if (!case_data->dflt) //default case handled in gen_switch_statement
	{
		gen_asm("%s:", case_data->sema.lbl);
		if (case_data->smnt)
			gen_statement(case_data->smnt);
	}
}

void gen_switch_statement(ast_statement_t* smnt)
{
	gen_annotate("switch statement");
	char lbl_end[MAX_LBL_LEN];

	gen_make_label_name(lbl_end);

	ast_switch_smnt_data_t* data = &smnt->data.switch_smnt;

	gen_expression(data->expr);

	for (uint32_t case_idx = 0; case_idx < data->sema.case_count; case_idx++)
	{
		ast_case_smnt_data_t* case_data = data->sema.case_smnts[case_idx];
		gen_asm("cmpl $%d, %%eax", int_val_as_uint32(&case_data->expr->data.int_literal.val));
		gen_asm("je %s", case_data->sema.lbl);
	}

	if (data->sema.dflt_case)
	{
		gen_asm("jmp %s", data->sema.dflt_case->sema.lbl);
	}
	else
	{
		gen_asm("jmp %s", lbl_end);
	}

	const char* prev_break_lbl = _cur_break_label;
	_cur_break_label = lbl_end;
	gen_statement(smnt->data.switch_smnt.smnt);

	if (data->sema.dflt_case)
	{
		gen_asm("%s:", data->sema.dflt_case->sema.lbl);
		gen_statement(data->sema.dflt_case->smnt);
	}
	gen_asm("%s:", lbl_end);
	_cur_break_label = prev_break_lbl;

	gen_annotate_end();
}

void gen_return_statement(ast_statement_t* smnt)
{
	gen_annotate_start("smnt_return");
	ast_type_spec_t* ret_type = _get_func_sig_ret_type(_cur_fun->type_ref);
	if (ret_type->size > 4)
	{
		gen_expression(smnt->data.expr);

		//the destination address is in the stack at stack_offset 9
		//load the address into edx
		gen_asm("movl 8(%%ebp), %%edx");

		//source address is in eax, dest in edx
		uint32_t offset = 0;
		while (offset < ret_type->size)
		{
			if (offset)
			{
				gen_asm("movl %d(%%eax), %%ecx", offset);
				gen_asm("movl %%ecx, %d(%%edx)", offset);
			}
			else
			{
				gen_asm("movl (%%eax), %%ecx");
				gen_asm("movl %%ecx, (%%edx)");
			}
			offset += 4;
		}
	}
	else
	{
		gen_expression(smnt->data.expr);
	}

	gen_annotate("epilogue");
	//function epilogue
	if (ret_type->size > 4)
	{
		gen_asm("movl 8(%%ebp), %%eax"); //restore return value address in eax
		gen_asm("leave");
		gen_asm("ret $4");
	}
	else
	{
		gen_asm("leave");
		gen_asm("ret");
	}
	_returned = true;
	gen_annotate_end();
}

void gen_statement(ast_statement_t* smnt)
{
	const char* cur_break = _cur_break_label;
	const char* cur_cont = _cur_cont_label;

	if (smnt->kind == smnt_switch)
	{
		gen_switch_statement(smnt);
	}
	else if (smnt->kind == smnt_case)
	{
		gen_case_statement(smnt);
	}
	else if (smnt->kind == smnt_return)
	{
		gen_return_statement(smnt);
	}
	else if (smnt->kind == smnt_expr)
	{
		gen_expression(smnt->data.expr);
	}
	else if (smnt->kind == smnt_if)
	{
		char label_false[16];
		char label_end[16];
		gen_make_label_name(label_false);
		gen_make_label_name(label_end);

		gen_expression(smnt->data.if_smnt.condition); //condition
		gen_asm("cmpl $0, %%eax"); //was false?
		gen_asm("je %s", label_false);
		gen_statement(smnt->data.if_smnt.true_branch); //true
		gen_asm("jmp %s", label_end);
		gen_asm("%s:", label_false);
		if (smnt->data.if_smnt.false_branch)
			gen_statement(smnt->data.if_smnt.false_branch);
		gen_asm("%s:", label_end);
	}
	else if (smnt->kind == smnt_compound)
	{
		gen_scope_block_enter();

		ast_block_item_t* blk = smnt->data.compound.blocks;
		while (blk)
		{
			gen_block_item(blk);
			blk = blk->next;
		}

		gen_scope_block_leave();
	}
	else if (smnt->kind == smnt_while)
	{
		char label_start[16];
		char label_end[16];
		gen_make_label_name(label_start);
		gen_make_label_name(label_end);

		_cur_break_label = label_end;
		_cur_cont_label = label_start;

		gen_asm("%s:", label_start);
		gen_expression(smnt->data.while_smnt.condition);
		gen_asm("cmpl $0, %%eax"); //was false?
		gen_asm("je %s", label_end);
		gen_statement(smnt->data.while_smnt.statement);
		gen_asm("jmp %s", label_start);
		gen_asm("%s:", label_end);
	}
	else if (smnt->kind == smnt_do)
	{
		char label_start[16];
		char label_end[16];
		char label_cont[16];
		gen_make_label_name(label_start);
		gen_make_label_name(label_end);
		gen_make_label_name(label_cont);

		_cur_break_label = label_end;
		_cur_cont_label = label_cont;

		gen_asm("%s:", label_start);
		gen_statement(smnt->data.while_smnt.statement);
		gen_asm("%s:", label_cont);
		gen_expression(smnt->data.while_smnt.condition);
		gen_asm("cmpl $0, %%eax"); //was false?
		gen_asm("je %s", label_end);
		gen_asm("jmp %s", label_start);
		gen_asm("%s:", label_end);
	}
	else if (smnt->kind == smnt_for)
	{
		ast_for_smnt_data_t* f_data = &smnt->data.for_smnt;

		char label_start[16];
		char label_end[16];
		char label_cont[16];
		gen_make_label_name(label_start);
		gen_make_label_name(label_end);
		gen_make_label_name(label_cont);

		gen_scope_block_enter();

		if (f_data->init)
			gen_expression(f_data->init);

		_cur_break_label = label_end;
		_cur_cont_label = label_cont;

		gen_asm("%s:", label_start);
		gen_expression(f_data->condition);
		gen_asm("cmpl $0, %%eax"); //was false?
		gen_asm("je %s", label_end);
		gen_statement(f_data->statement);
		gen_asm("%s:", label_cont);
		if (f_data->post)
			gen_expression(f_data->post);
		gen_asm("jmp %s", label_start);
		gen_asm("%s:", label_end);

		gen_scope_block_leave();
	}
	else if (smnt->kind == smnt_for_decl)
	{
		ast_for_smnt_data_t* f_data = &smnt->data.for_smnt;

		char label_start[16];
		char label_end[16];
		char label_cont[16];
		gen_make_label_name(label_start);
		gen_make_label_name(label_end);
		gen_make_label_name(label_cont);

		gen_scope_block_enter();

		ast_declaration_t* decl = f_data->decls.first;
		while (decl)
		{
			gen_var_decl(decl);
			decl = decl->next;
		}

		_cur_break_label = label_end;
		_cur_cont_label = label_cont;

		gen_asm("%s:", label_start);
		gen_expression(f_data->condition);
		gen_asm("cmpl $0, %%eax"); //was false?
		gen_asm("je %s", label_end);
		gen_statement(f_data->statement);
		gen_asm("%s:", label_cont);
		if (f_data->post)
			gen_expression(f_data->post);
		gen_asm("jmp %s", label_start);
		gen_asm("%s:", label_end);
		gen_scope_block_leave();
	}
	else if (smnt->kind == smnt_break)
	{
		if (!_cur_break_label)
			diag_err(smnt->tokens.start, ERR_SYNTAX, "Invalid break");
		gen_asm("jmp %s", _cur_break_label);
	}
	else if (smnt->kind == smnt_continue)
	{
		if (!_cur_cont_label)
			diag_err(smnt->tokens.start, ERR_SYNTAX, "Invalid continue");
		gen_asm("jmp %s", _cur_cont_label);
	}
	else if (smnt->kind == smnt_label)
	{
		gen_asm("%s:", smnt->data.label_smnt.label);
		gen_statement(smnt->data.label_smnt.smnt);
	}
	else if (smnt->kind == smnt_goto)
	{
		gen_asm("jmp %s", smnt->data.goto_smnt.label);
	}

	_cur_break_label = cur_break;
	_cur_cont_label = cur_cont;
}

void gen_block_item(ast_block_item_t* bi)
{
	if (bi->kind == blk_smnt)
	{
		gen_statement(bi->data.smnt);
	}
	else if (bi->kind == blk_decl)
	{
		ast_declaration_t* decl = bi->data.decls.first;
		while (decl)
		{
			if (decl->kind == decl_var)
				gen_var_decl(decl);
			decl = decl->next;
		}
	}
}

void gen_function(ast_declaration_t* fn)
{
	_returned = false;
	_cur_fun = fn;

	gen_annotate_start("function '%s'", fn->name);
	gen_asm(".globl %s", fn->name);
	gen_asm("%s:", fn->name);

	gen_annotate("prologue");
	gen_asm("push %%ebp");
	gen_asm("movl %%esp, %%ebp");

	if (fn->data.func.sema.required_stack_size > 0)
	{
		//align stack on 4 byte boundry
		uint32_t sz = (fn->data.func.sema.required_stack_size + 0x03) & ~0x03;
		gen_asm("subl $%d, %%esp", sz);
	}

	ast_block_item_t* blk = fn->data.func.blocks;

	while (blk)
	{
		gen_block_item(blk);
		blk = blk->next;
	}

	if (!_returned)
	{
		gen_annotate("epilogue");
		//function epilogue
		gen_asm("movl $0, %%eax");
		gen_asm("leave");

		gen_asm("ret");
	}
	gen_annotate_end();
	gen_asm("\n");
	gen_asm("\n");

	_cur_fun = NULL;
}

void gen_global_var_init(ast_expression_t* expr, ast_type_spec_t* spec);

void gen_global_compount_init(ast_expression_t* expr, ast_type_spec_t* spec)
{
	if (ast_type_is_array(spec))
	{
		ast_compound_init_item_t* init_item = expr->data.compound_init.item_list;
		ast_type_spec_t* elem_type = spec->data.array_spec->element_type;
		while (init_item)
		{
			gen_global_var_init(init_item->expr, elem_type);
			init_item = init_item->next;
		}
	}
	else if (ast_type_is_struct_union(spec))
	{
		ast_compound_init_item_t* init_item = expr->data.compound_init.item_list;
		size_t offset = 0;
		while (init_item)
		{
			ast_struct_member_t* member = init_item->sema.member;
			if (offset != member->sema.offset)
			{
				//padding
				gen_asm(".zero %d", member->sema.offset - offset);
				offset = member->sema.offset;
			}

			gen_global_var_init(init_item->expr, member->decl->type_ref->spec);

			offset += member->decl->type_ref->spec->size;

			init_item = init_item->next;
		}

		if (offset != expr->sema.result.type->size)
		{
			//padding
			gen_asm(".zero %d", expr->sema.result.type->size - offset);
		}
	}
}

void gen_global_var_init(ast_expression_t* expr, ast_type_spec_t* spec)
{
	if (expr->kind == expr_int_literal)
	{
		assert(int_val_required_width(&expr->data.int_literal.val) <= 32);
		if (spec->size == 1)
			gen_asm(".byte %d", int_val_as_int32(&expr->data.int_literal.val));
		else if (spec->size == 2)
			gen_asm(".value %d", int_val_as_int32(&expr->data.int_literal.val));
		else if (spec->size == 4)
		{
			if (expr->data.int_literal.val.is_signed)
				gen_asm(".long %d", int_val_as_int32(&expr->data.int_literal.val)); //data and init value
			else
				gen_asm(".long %d", int_val_as_uint32(&expr->data.int_literal.val)); //data and init value*/
		}
	}
	else if (expr->kind == expr_str_literal)
	{
		gen_asm(".long .%s", expr->data.str_literal.sema.label); //label
	}
	else if (expr->kind == expr_compound_init)
	{
		gen_global_compount_init(expr, spec);
	}
}

void gen_global_var(ast_declaration_t* decl)
{
	gen_annotate_start("global variable '%s'", decl->name);

	var_data_t* var = var_decl_global_var(_var_set, decl);

	gen_asm(".globl %s", var->global_name); //export symbol
	ast_expression_t* var_expr = decl->data.var.init_expr;
	if (var_expr)
	{
		gen_annotate("initialisation");
		//initialised var goes in .data section
		gen_asm(".data"); //data section
		gen_asm(".align 4");
		gen_asm("%s:", var->global_name); //label
		gen_global_var_init(var_expr, decl->type_ref->spec);

		gen_asm(".text"); //back to text section
		gen_asm("\n");
	}
	else
	{
		gen_annotate("init with zeros");
		//0 or uninitialised var goes in .BSS section
		gen_asm(".bss"); //bss section
		gen_asm(".align 4");
		gen_asm("%s:", var->global_name); //label
		gen_asm(".zero %d", decl->type_ref->spec->size); //data length
		gen_asm(".text"); //back to text section
		gen_asm("\n");
	}
	gen_annotate_end();
}

void gen_string_literal(const char* value, const char* label)
{
	gen_annotate_start("string literal");
	gen_asm(".%s:", label);
	gen_asm(".string \"%s\"", value);
	gen_asm("\n");
	gen_annotate_end();
}

void code_gen(valid_trans_unit_t* tl, write_asm_cb cb, void* data, bool annotation)
{
	_asm_cb = cb;
	_asm_cb_data = data;
	_cur_tl = tl;
	_annotation = annotation;
	_var_set = var_init_set();

	tl_decl_t* var_decl = tl->var_decls;
	while (var_decl)
	{
		
		gen_global_var(var_decl->decl);
		var_decl = var_decl->next;
	}

	sht_iterator_t it = sht_begin(tl->string_literals);
	while (!sht_end(tl->string_literals, &it))
	{
		gen_string_literal(it.key, (const char*)it.val);
		sht_next(tl->string_literals, &it);
	}

	tl_decl_t* fn_decl = tl->fn_decls;
	while (fn_decl)
	{
		if (fn_decl->decl->data.func.blocks)
		{
			var_enter_function(_var_set, fn_decl->decl);
			gen_function(fn_decl->decl);
			var_leave_function(_var_set);
		}
		fn_decl = fn_decl->next;
	}
	var_destory_set(_var_set);
	_cur_tl = NULL;
	_var_set = NULL;
	_asm_cb_data = NULL;
	_asm_cb = NULL;
}