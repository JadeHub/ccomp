#include "code_gen.h"

#include <stdio.h>
#include <stdarg.h>

static write_asm_cb _asm_cb;

static void _gen_asm(const char* format, ...)
{
	char buff[256];

	va_list args;
	va_start(args, format);
	vsnprintf(buff, 256, format, args);
	va_end(args);

	_asm_cb(buff);
}

void gen_expression(ast_expression_t* expr)
{
	if (expr->kind == expr_int_literal)
	{
		_gen_asm("movl $%d, %%eax", expr->data.const_val); //movl $3, %eax
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
			//_gen_asm("movl %%ecx, %%eax");
			break;
		case op_mul:
			_gen_asm("imul %%ecx, %%eax");
			break;
		case op_div:
			_gen_asm("xor %%edx, %%edx");
			_gen_asm("cdq");
			_gen_asm("idivl %%ecx");
			break;
		}
	}
}

void gen_statement(ast_statement_t* stat)
{
	//if, break, continue, while etc
	//if (stat.kind == return)
	{
		gen_expression(stat->expr);
		_gen_asm("ret");

		
	}
}

void gen_function(ast_function_decl_t* fn)
{
	_gen_asm(".globl %s", fn->name);
	_gen_asm("%s:", fn->name);

	gen_statement(fn->return_statement);
}

void code_gen(ast_trans_unit_t* ast, write_asm_cb cb)
{
	_asm_cb = cb;

	gen_function(ast->function);
}