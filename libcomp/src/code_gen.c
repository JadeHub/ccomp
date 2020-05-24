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

void gen_statement(ast_statement_t* stat)
{
	//return
	_gen_asm("movl $%d, %%eax", stat->val.val); //movl $3, %eax
	_gen_asm("ret");
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