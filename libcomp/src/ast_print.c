#include "ast.h"

#include <stdio.h>

static uint32_t _cur_indent = 0;

static void _print_indent()
{
	for (uint32_t i = 0; i < _cur_indent; i++)
	{
		printf("  ");
	}
}

static const char* _op_to_str(op_kind k)
{
	switch (k)
	{
	case op_sub:
		return "[-] Subtraction";
	case op_negate:
		return "[-] Negation";
	case op_compliment:
		return "[~] Compliment";
	case op_not:
		return "[!] Not";
	case op_add:
		return "[+] Addition";
	case op_mul:
		return "[*] Multiplication";
	case op_div:
		return "[/] Division";
	}
	return "ERROR";
}

void ast_print_expression(ast_expression_t* expr, const char* prefix)
{
	_print_indent();

	switch (expr->kind)
	{
		case expr_binary_op:
			printf("%sBinaryOp %s\n", prefix, _op_to_str(expr->data.binary_op.operation));
			_cur_indent++;
			ast_print_expression(expr->data.binary_op.lhs, "lhs: ");
			ast_print_expression(expr->data.binary_op.rhs, "rhs: ");
			_cur_indent--;
			break;
		case expr_unary_op:
			printf("%sUnaryOp %s\n", prefix, _op_to_str(expr->data.unary_op.operation));
			_cur_indent++;
			ast_print_expression(expr->data.unary_op.expression, "");
			_cur_indent--;
			break;
		case expr_int_literal:
			printf("%sIntegerLiteral %d\n", prefix, expr->data.const_val);
			break;
	}
}

void ast_print_statement(ast_statement_t* s)
{
	_print_indent();
	printf("ReturnStatement\n");
	_cur_indent++;
	ast_print_expression(s->expr, "");
	_cur_indent--;
}

void ast_print_function(ast_function_decl_t* f)
{
	_print_indent();
	printf("FunctionDecl %s\n", f->name);
	_cur_indent++;
	ast_print_statement(f->return_statement);
	_cur_indent--;
}

void ast_print(ast_trans_unit_t* tl)
{
	_cur_indent = 0;
	ast_print_function(tl->function);

}