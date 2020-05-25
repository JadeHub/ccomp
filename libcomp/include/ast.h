#pragma once

#include <stdint.h>

struct ast_expression;

typedef enum
{
	//unary operators
	op_negate,
	op_compliment,
	op_not,
	//binary operators
	op_add,
	op_sub,
	op_mul,
	op_div

}op_kind;

typedef struct
{
	op_kind operation;
	struct ast_expression* expression;
}unary_op_expr_data_t;

typedef struct
{
	op_kind operation;
	struct ast_expression* lhs;
	struct ast_expression* rhs;
}binary_op_expr_data_t;

typedef enum
{
	expr_unary_op,
	expr_binary_op,
	expr_int_literal

}expression_kind;

typedef struct ast_expression
{
	expression_kind kind;

	union
	{
		unary_op_expr_data_t unary_op;
		binary_op_expr_data_t binary_op;
		uint32_t const_val;
	}data;
}ast_expression_t;


typedef struct
{
	//type (return)..for, while etc

	ast_expression_t* expr;

}ast_statement_t;

typedef struct
{
	char name[32];

	//params
	//return type
	//static?

	ast_statement_t* return_statement;

}ast_function_decl_t;

typedef struct
{
	ast_function_decl_t* function;
}ast_trans_unit_t;


void ast_print(ast_trans_unit_t* tl);