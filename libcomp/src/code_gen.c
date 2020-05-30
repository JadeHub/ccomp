#include "code_gen.h"
#include "var_set.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>

void gen_expression(ast_expression_t* expr);
void gen_block_item(ast_block_item_t* bi);

static write_asm_cb _asm_cb;
static diag_cb _diag_cb;

static var_set_t* _var_set;
static bool _returned = false;

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

void gen_expression(ast_expression_t* expr)
{
	if (expr->kind == expr_condition)
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
		stack_var_data_t* var = var_find(_var_set, expr->data.assignment.name);
		if (!var)
		{
			exit(1);
		}
		gen_expression(expr->data.assignment.expr);
		_gen_asm("movl %%eax, %d(%%ebp)", var->bsp_offset);
	}
	else if (expr->kind == expr_var_ref)
	{
		stack_var_data_t* var = var_find(_var_set, expr->data.assignment.name);
		if (!var)
		{
			exit(1);
		}
		_gen_asm("movl %d(%%ebp), %%eax", var->bsp_offset);
	}
	else if (expr->kind == expr_int_literal)
	{
		_gen_asm("movl $%d, %%eax", expr->data.const_val);
	}
	else if (expr->kind == expr_unary_op)
	{
		gen_expression(expr->data.unary_op.expression);
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

void gen_statement(ast_statement_t* smnt)
{
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
		var_enter_block(_var_set);

		ast_block_item_t* blk = smnt->data.compound.blocks;
		while (blk)
		{
			gen_block_item(blk);
			blk = blk->next;
		}

		int bsp = var_leave_block(_var_set);
		_gen_asm("addl $%d, %%esp", bsp);
	}
	//if, break, continue, while etc
}

void gen_block_item(ast_block_item_t* bi)
{
	if (bi->kind == blk_smnt)
	{
		gen_statement(bi->smnt);
	}
	else if (bi->kind == blk_var_def)
	{
		stack_var_data_t* var = var_decl_stack_var(_var_set, bi->var_decl);
		if (bi->var_decl->expr)
		{
			gen_expression(bi->var_decl->expr);
			_gen_asm("pushl %%eax");
		}
		else
		{
			_gen_asm("pushl $0");
		}
	}
}

void gen_function(ast_function_decl_t* fn)
{
	var_enter_function(_var_set, fn);

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

	var_leave_function(_var_set);
}

void code_gen(ast_trans_unit_t* ast, write_asm_cb cb, diag_cb dcb)
{
	_asm_cb = cb;
	_diag_cb = dcb;

	_var_set = var_init_set(_diag_cb);

	gen_function(ast->function);

	free(_var_set);
}