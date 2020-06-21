#include "code_gen.h"
#include "var_set.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>

void gen_expression(ast_expression_t* expr);
void gen_block_item(ast_block_item_t* bi);

static write_asm_cb _asm_cb;

static var_set_t* _var_set;
static bool _returned = false;

static const char* _cur_break_label = NULL;
static const char* _cur_cont_label = NULL;

static void _gen_asm(const char* format, ...)
{
	char buff[256];

	va_list args;
	va_start(args, format);
	vsnprintf(buff, 256, format, args);
	va_end(args);

	_asm_cb(buff);
}

static uint32_t _next_label = 0;

static void _make_label_name(char* name)
{
	sprintf(name, "_label%d", ++_next_label);
}

void gen_logical_binary_expr(ast_expression_t* expr)
{
	char label1[16];
	char label2[16];
	_make_label_name(label1);
	_make_label_name(label2);

	gen_expression(expr->data.binary_op.lhs);

	if (expr->data.binary_op.operation == op_or)
	{
		_gen_asm("cmpl $0, %%eax"); //compare eax to 0
		_gen_asm("je %s", label1);	//eax is the result of the first expression. If true we exit early
		_gen_asm("movl $1, %%eax"); //set eax to 1 (result)
		_gen_asm("jmp %s", label2);

		_gen_asm("%s:", label1);
		gen_expression(expr->data.binary_op.rhs);
		_gen_asm("cmpl $0, %%eax"); //compare eax to 0
		_gen_asm("movl $0, %%eax"); //set eax to 0
		_gen_asm("setne %%al"); //test flags of comparison
		_gen_asm("%s:", label2);
	}
	else if (expr->data.binary_op.operation == op_and)
	{
		_gen_asm("cmpl $0, %%eax"); //compare eax to 0
		_gen_asm("jne %s", label1);
		//eax already 0 (the result)
		_gen_asm("jmp %s", label2); //exit early
		
		_gen_asm("%s:", label1);
		gen_expression(expr->data.binary_op.rhs);
		_gen_asm("cmpl $0, %%eax"); //compare eax to 0
		_gen_asm("movl $0, %%eax"); //set eax to 0
		_gen_asm("setne %%al"); //test flags of comparison
		_gen_asm("%s:", label2);
	}
}

void gen_assign_expression(token_t* tok, const char* var_name)
{
	var_data_t* var = var_find(_var_set, var_name);
	if(!var)
	{
		diag_err(tok, ERR_UNKNOWN_VAR, "assignment to unknown variabe %s", var_name);
		return;
	}
	if (var->bsp_offset != 0)
		_gen_asm("movl %%eax, %d(%%ebp)", var->bsp_offset);
	else //global
		_gen_asm("movl %%eax, _var_%s", var->name);
}

void gen_expression(ast_expression_t* expr)
{
	if (expr->kind == expr_null)
	{
		//null
	}
	else if (expr->kind == expr_func_call)
	{
		ast_expression_t* param = expr->data.func_call.params;

		while (param)
		{
			gen_expression(param);
			_gen_asm("pushl %%eax");
			param = param->next;
		}
		_gen_asm("call %s", expr->data.func_call.name);
		_gen_asm("addl $%d, %%esp", expr->data.func_call.param_count * 4);
	}
	else if (expr->kind == expr_condition)
	{
		char label_false[16];
		char label_end[16];
		_make_label_name(label_false);
		_make_label_name(label_end);

		gen_expression(expr->data.condition.cond); //condition
		_gen_asm("cmpl $0, %%eax"); //was false?
		_gen_asm("je %s", label_false);
		gen_expression(expr->data.condition.true_branch); //true
		_gen_asm("jmp %s", label_end);
		_gen_asm("%s:", label_false);
		gen_expression(expr->data.condition.false_branch);
		_gen_asm("%s:", label_end);
	}
	else if (expr->kind == expr_assign)
	{
		if (expr->data.assignment.target->kind == expr_var_ref)
		{
			gen_expression(expr->data.assignment.expr);
			gen_assign_expression(expr->tokens.start, expr->data.assignment.target->data.var_reference.name);
		}
	}
	else if (expr->kind == expr_var_ref)
	{
		var_data_t* var = var_find(_var_set, expr->data.var_reference.name);
		
		if (!var)
		{
			diag_err(expr->tokens.start, ERR_UNKNOWN_VAR, "unknown variabe reference %s", expr->data.var_reference.name);
			return;
		}

		if (var->bsp_offset != 0)
			_gen_asm("movl %d(%%ebp), %%eax", var->bsp_offset);
		else //global
			_gen_asm("movl _var_%s, %%eax", var->name);
	}
	else if (expr->kind == expr_int_literal)
	{
		_gen_asm("movl $%d, %%eax", expr->data.const_val);
	}
	else if (expr->kind == expr_unary_op)
	{
		ast_expression_t* param = expr->data.unary_op.expression;
		gen_expression(param);
		switch (expr->data.unary_op.operation)
		{
		case op_negate:
			_gen_asm("neg %%eax");
			break;
		case op_compliment:
			_gen_asm("not %%eax");
			break;
		case op_not:
			_gen_asm("cmpl $0, %%eax"); //compare eax to 0
			_gen_asm("movl $0, %%eax"); //set eax to 0
			_gen_asm("sete %%al"); //if eax was 0 in cmpl, set al to 1
			break;
		case op_prefix_inc:
			assert(param->kind == expr_var_ref);
			_gen_asm("incl %%eax");
			gen_assign_expression(expr->tokens.start, param->data.var_reference.name);
			break;
		case op_prefix_dec:
			assert(expr->data.unary_op.expression->kind == expr_var_ref);
			_gen_asm("decl %%eax");
			gen_assign_expression(expr->tokens.start, param->data.var_reference.name);
			break;
		}
	}
	else if (expr->kind == expr_postfix_op)
	{
		assert(expr->data.unary_op.expression->kind == expr_var_ref);
		ast_expression_t* param = expr->data.unary_op.expression;
		switch (expr->data.unary_op.operation)
		{
		case op_postfix_inc:
			//get the value of the parameter
			gen_expression(param);
			//save the current value
			_gen_asm("pushl %%eax");
			//increment
			_gen_asm("incl %%eax");
			//assign incremented value back to variable
			gen_assign_expression(expr->tokens.start, param->data.var_reference.name);
			//pop the un-incremented value back into eax
			_gen_asm("popl %%eax");
			break;
		case op_postfix_dec:
			gen_expression(param);
			_gen_asm("pushl %%eax");
			_gen_asm("decl %%eax");
			gen_assign_expression(expr->tokens.start, param->data.var_reference.name);
			_gen_asm("popl %%eax");
			break;
		}
	}
	else if (expr->kind == expr_binary_op)
	{
		if (expr->data.binary_op.operation == op_and ||
			expr->data.binary_op.operation == op_or)
		{
			gen_logical_binary_expr(expr);
			return;
		}
		gen_expression(expr->data.binary_op.rhs);
		_gen_asm("push %%eax");
		gen_expression(expr->data.binary_op.lhs);
		_gen_asm("pop %%ecx");

		switch (expr->data.binary_op.operation)
		{
		case op_add:
			_gen_asm("addl %%ecx, %%eax");
			break;
		case op_sub:
			_gen_asm("subl %%ecx, %%eax");
			break;
		case op_mul:
			_gen_asm("imul %%ecx, %%eax");
			break;
		case op_div:
			_gen_asm("xor %%edx, %%edx");
			_gen_asm("cdq");
			_gen_asm("idivl %%ecx");
			break;
		case op_shiftleft:
			_gen_asm("sall %%cl, %%eax");
			break;
		case op_shiftright:
			_gen_asm("sarl %%cl, %%eax");
			break;
		case op_eq:
			_gen_asm("cmpl %%ecx, %%eax"); //compare eax to ecx
			_gen_asm("movl $0, %%eax"); //set eax to 0
			_gen_asm("sete %%al"); //test flags of comparison
			break;
		case op_neq:
			_gen_asm("cmpl %%ecx, %%eax"); //compare eax to ecx
			_gen_asm("movl $0, %%eax"); //set eax to 0
			_gen_asm("setne %%al"); //test flags of comparison
			break;
		case op_greaterthanequal:
			_gen_asm("cmpl %%ecx, %%eax"); //compare eax to ecx
			_gen_asm("movl $0, %%eax"); //set eax to 0
			_gen_asm("setge %%al"); //test flags of comparison
			break;
		case op_greaterthan:
			_gen_asm("cmpl %%ecx, %%eax"); //compare eax to ecx
			_gen_asm("movl $0, %%eax"); //set eax to 0
			_gen_asm("setg %%al"); //test flags of comparison
			break;
		case op_lessthanequal:
			_gen_asm("cmpl %%ecx, %%eax"); //compare eax to ecx
			_gen_asm("movl $0, %%eax"); //set eax to 0
			_gen_asm("setle %%al"); //test flags of comparison
			break;
		case op_lessthan:
			_gen_asm("cmpl %%ecx, %%eax"); //compare eax to ecx
			_gen_asm("movl $0, %%eax"); //set eax to 0
			_gen_asm("setl %%al"); //test flags of comparison
			break;
		case op_bitwise_and:
			_gen_asm("andl %%ecx, %%eax"); //and eax to ecx, store in eax
			break;
		case op_bitwise_or:
			_gen_asm("orl %%ecx, %%eax"); //and eax to ecx, store in eax
			break;
		case op_bitwise_xor:
			_gen_asm("xorl %%ecx, %%eax"); //and eax to ecx, store in eax
			break;
		case op_mod:
			_gen_asm("xor %%edx, %%edx");
			_gen_asm("cdq");
			_gen_asm("idivl %%ecx");
			_gen_asm("movl %%edx, %%eax");
			break;
		}
	}
}

void gen_var_decl(ast_var_decl_t* var_decl)
{
	var_data_t* var = var_decl_stack_var(_var_set, var_decl);
	assert(var);
	if (var_decl->expr)
	{
		gen_expression(var_decl->expr);
		_gen_asm("pushl %%eax");
	}
	else
	{
		_gen_asm("pushl $0");
	}
}

void gen_scope_block_enter()
{
	var_enter_block(_var_set);	
}

void gen_scope_block_leave()
{
	int bsp = var_leave_block(_var_set);
	_gen_asm("addl $%d, %%esp", bsp);
}

void gen_statement(ast_statement_t* smnt)
{
	const char* cur_break = _cur_break_label;
	const char* cur_cont = _cur_cont_label;

	if (smnt->kind == smnt_return)
	{
		gen_expression(smnt->data.expr);

		//function epilogue
		_gen_asm("movl %%ebp, %%esp");
		_gen_asm("pop %%ebp");

		_gen_asm("ret");		
		_returned = true;
	}
	else if (smnt->kind == smnt_expr)
	{
		gen_expression(smnt->data.expr);
	}
	else if (smnt->kind == smnt_if)
	{
		char label_false[16];
		char label_end[16];
		_make_label_name(label_false);
		_make_label_name(label_end);

		gen_expression(smnt->data.if_smnt.condition); //condition
		_gen_asm("cmpl $0, %%eax"); //was false?
		_gen_asm("je %s", label_false);
		gen_statement(smnt->data.if_smnt.true_branch); //true
		_gen_asm("jmp %s", label_end);
		_gen_asm("%s:", label_false);
		if(smnt->data.if_smnt.false_branch)
			gen_statement(smnt->data.if_smnt.false_branch);
		_gen_asm("%s:", label_end);
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
		_make_label_name(label_start);
		_make_label_name(label_end);

		_cur_break_label = label_end;
		_cur_cont_label = label_start;

		_gen_asm("%s:", label_start);
		gen_expression(smnt->data.while_smnt.condition);
		_gen_asm("cmpl $0, %%eax"); //was false?
		_gen_asm("je %s", label_end);
		gen_statement(smnt->data.while_smnt.statement);
		_gen_asm("jmp %s", label_start);
		_gen_asm("%s:", label_end);
	}
	else if (smnt->kind == smnt_do)
	{
		char label_start[16];
		char label_end[16];
		char label_cont[16];
		_make_label_name(label_start);
		_make_label_name(label_end);
		_make_label_name(label_cont);

		_cur_break_label = label_end;
		_cur_cont_label = label_cont;

		_gen_asm("%s:", label_start);
		gen_statement(smnt->data.while_smnt.statement);
		_gen_asm("%s:", label_cont);
		gen_expression(smnt->data.while_smnt.condition);
		_gen_asm("cmpl $0, %%eax"); //was false?
		_gen_asm("je %s", label_end);
		_gen_asm("jmp %s", label_start);
		_gen_asm("%s:", label_end);
	}
	else if (smnt->kind == smnt_for)
	{
		ast_for_smnt_data_t* f_data = &smnt->data.for_smnt;

		char label_start[16];
		char label_end[16];
		char label_cont[16];
		_make_label_name(label_start);
		_make_label_name(label_end);
		_make_label_name(label_cont);

		gen_scope_block_enter();

		if (f_data->init)
			gen_expression(f_data->init);

		_cur_break_label = label_end;
		_cur_cont_label = label_cont;

		_gen_asm("%s:", label_start);
		gen_expression(f_data->condition);
		_gen_asm("cmpl $0, %%eax"); //was false?
		_gen_asm("je %s", label_end);
		gen_statement(f_data->statement);
		_gen_asm("%s:", label_cont);
		if (f_data->post)
			gen_expression(f_data->post);
		_gen_asm("jmp %s", label_start);
		_gen_asm("%s:", label_end);
		gen_scope_block_leave();
	}
	else if (smnt->kind == smnt_for_decl)
	{
		ast_for_smnt_data_t* f_data = &smnt->data.for_smnt;

		char label_start[16];
		char label_end[16];
		char label_cont[16];
		_make_label_name(label_start);
		_make_label_name(label_end);
		_make_label_name(label_cont);

		gen_scope_block_enter();

		if (f_data->init_decl)
			gen_var_decl(&f_data->init_decl->data.var);

		_cur_break_label = label_end;
		_cur_cont_label = label_cont;

		_gen_asm("%s:", label_start);
		gen_expression(f_data->condition);
		_gen_asm("cmpl $0, %%eax"); //was false?
		_gen_asm("je %s", label_end);
		gen_statement(f_data->statement);
		_gen_asm("%s:", label_cont);
		if (f_data->post)
			gen_expression(f_data->post);
		_gen_asm("jmp %s", label_start);
		_gen_asm("%s:", label_end);
		gen_scope_block_leave();
	}
	else if (smnt->kind == smnt_break)
	{
		if (!_cur_break_label)
			diag_err(smnt->tokens.start, ERR_SYNTAX, "Invalid break");
		_gen_asm("jmp %s", _cur_break_label);
	}
	else if (smnt->kind == smnt_continue)
	{
	if (!_cur_cont_label)
		diag_err(smnt->tokens.start, ERR_SYNTAX, "Invalid continue");
	_gen_asm("jmp %s", _cur_cont_label);
	}
	//if, break, continue,  etc

	_cur_break_label = cur_break;
	_cur_cont_label = cur_cont;
}

void gen_block_item(ast_block_item_t* bi)
{
	if (bi->kind == blk_smnt)
	{
		gen_statement(bi->smnt);
	}
	else if (bi->kind == blk_decl)
	{
		if(bi->decl->kind == decl_var)
			gen_var_decl(&bi->decl->data.var);
	}
}

void gen_function(ast_function_decl_t* fn)
{	
	_returned = false;

	_gen_asm(".globl %s", fn->name);
	_gen_asm("%s:", fn->name);

	//function prologue
	_gen_asm("push %%ebp");
	_gen_asm("movl %%esp, %%ebp");

	ast_block_item_t* blk = fn->blocks;

	while (blk)
	{
		gen_block_item(blk);
		blk = blk->next;
	}

	if (!_returned)
	{
		//function epilogue
		_gen_asm("movl $0, %%eax");
		_gen_asm("movl %%ebp, %%esp");
		_gen_asm("pop %%ebp");

		_gen_asm("ret");
	}
	_gen_asm("\n");
	_gen_asm("\n");
}

void gen_global_var(ast_var_decl_t* var)
{
	_gen_asm(".globl _var_%s", var->name); //export symbol
	if (var->expr && var->expr->data.const_val != 0)
	{
		//initialised var goes in .data section
		_gen_asm(".data"); //data section
		_gen_asm(".align 4");
		_gen_asm("_var_%s:", var->name); //label
		_gen_asm(".long %d", var->expr->data.const_val); //data and init value
	}
	else
	{
		//0 or uninitialised var goes in .BSS section
		_gen_asm(".bss"); //bss section
		_gen_asm(".align 4");
		_gen_asm("_var_%s:", var->name); //label
		_gen_asm(".zero 4"); //data length		
	}
	_gen_asm(".text"); //back to text section
	_gen_asm("\n");
}

void code_gen(ast_trans_unit_t* ast, write_asm_cb cb)
{
	_asm_cb = cb;
	_var_set = var_init_set();

	ast_declaration_t* decl = ast->decls;

	while (decl)
	{
		if (decl->kind == decl_func && decl->data.func.blocks)
		{
			var_enter_function(_var_set, &decl->data.func);
			gen_function(&decl->data.func);
			var_leave_function(_var_set);
		}
		else if (decl->kind == decl_var)
		{
			var_decl_global_var(_var_set, &decl->data.var);
			gen_global_var(&decl->data.var);
		}
		decl = decl->next;
	}
	var_destory_set(_var_set);
}