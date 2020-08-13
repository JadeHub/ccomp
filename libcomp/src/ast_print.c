#include "ast.h"

#include <stdio.h>

#if 0

static uint32_t _cur_indent = 0;


static void _print_indent()
{
	for (uint32_t i = 0; i < _cur_indent; i++)
	{
		printf("  ");
	}
}

const char* ast_op_name(op_kind k)
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
	case op_and:
		return "[&&] And";
	case op_or:
		return "[||] Or";
	case op_eq:
		return "[==] Equal";
	case op_neq:
		return "[!=] Not equal";
	case op_lessthan:
		return "[<] Less than";
	case op_lessthanequal:
		return "[<=] Less than or equal";
	case op_greaterthan:
		return "[>] Greater than";
	case op_greaterthanequal:
		return "[>] Greater than or equal";
	case op_shiftleft:
		return "[<<] Shift left";
	case op_shiftright:
		return "[>>] Shift right";
	case op_bitwise_and:
		return "[&] Bitwise and";
	case op_bitwise_or:
		return "[|] Bitwise or";
	case op_bitwise_xor:
		return "[^] Bitwise xor";
	case op_mod:
		return "[%] Modulo";
	case op_prefix_inc:
		return "[++] Prefix inc";
	case op_prefix_dec:
		return "[-] Prefix dec";
	case op_postfix_inc:
		return "[++] Postfix inc";
	case op_postfix_dec:
		return "[--] Postfix inc";
	}
	return "ERROR";
}

void ast_print_expression(ast_expression_t* expr, const char* prefix)
{
	_print_indent();

	switch (expr->kind)
	{
		case expr_binary_op:
			printf("%sBinaryOp %s\n", prefix, ast_op_name(expr->data.binary_op.operation));
			_cur_indent++;
			ast_print_expression(expr->data.binary_op.lhs, "lhs: ");
			ast_print_expression(expr->data.binary_op.rhs, "rhs: ");
			_cur_indent--;
			break;
		case expr_unary_op:
			printf("%sUnaryOp %s\n", prefix, ast_op_name(expr->data.unary_op.operation));
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
	/*_print_indent();
	printf("ReturnStatement\n");
	_cur_indent++;
	ast_print_expression(s->data.expr, "");
	_cur_indent--;*/
}

void ast_print_function(ast_function_decl_t* f)
{
	/*_print_indent();
	printf("FunctionDecl %s\n", f->name);
	_cur_indent++;
	ast_print_statement(f->statements);
	_cur_indent--;*/
}

void ast_print(ast_trans_unit_t* tl)
{
	_cur_indent = 0;
	ast_print_function(tl->functions);

}

#endif

const char* ast_op_name(op_kind k)
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
	case op_and:
		return "[&&] And";
	case op_or:
		return "[||] Or";
	case op_eq:
		return "[==] Equal";
	case op_neq:
		return "[!=] Not equal";
	case op_lessthan:
		return "[<] Less than";
	case op_lessthanequal:
		return "[<=] Less than or equal";
	case op_greaterthan:
		return "[>] Greater than";
	case op_greaterthanequal:
		return "[>] Greater than or equal";
	case op_shiftleft:
		return "[<<] Shift left";
	case op_shiftright:
		return "[>>] Shift right";
	case op_bitwise_and:
		return "[&] Bitwise and";
	case op_bitwise_or:
		return "[|] Bitwise or";
	case op_bitwise_xor:
		return "[^] Bitwise xor";
	case op_mod:
		return "[%] Modulo";
	case op_prefix_inc:
		return "[++] Prefix inc";
	case op_prefix_dec:
		return "[-] Prefix dec";
	case op_postfix_inc:
		return "[++] Postfix inc";
	case op_postfix_dec:
		return "[--] Postfix inc";
	case op_member_access:
		return "[.] Member access";
	case op_address_of:
		return "[&] Address of";
	case op_dereference:
		return "[*] Defererence";
	}
	return "ERROR";
}

