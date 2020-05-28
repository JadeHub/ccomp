#pragma once

#include <stdint.h>
#include "token.h"

struct ast_expression;

#define MAX_LITERAL_NAME 32

typedef enum
{
	op_unknown,
	//unary operators
	op_negate,
	op_compliment,
	op_not,
	//binary operators
	op_add,
	op_sub,
	op_mul,
	op_div,
	op_mod,

	op_shiftleft,
	op_shiftright,

	op_bitwise_or,
	op_bitwise_xor,
	op_bitwise_and,

	//logical operators
	op_and,
	op_or,
	op_eq,
	op_neq,
	op_lessthan,
	op_lessthanequal,
	op_greaterthan,
	op_greaterthanequal,

}op_kind;

/* Expressions */

/*
Unary operation expression data
eg !x
*/
typedef struct
{
	op_kind operation;
	struct ast_expression* expression;
}unary_op_expr_data_t;

/*
Binary operation expression data
eg x + y
*/
typedef struct
{
	op_kind operation;
	struct ast_expression* lhs;
	struct ast_expression* rhs;
}binary_op_expr_data_t;

/*
assignment expression data
eg x = 5
*/
typedef struct
{
	char name[MAX_LITERAL_NAME];
	struct ast_expression* expr;
}assign_expr_data_t;

/*
variable reference data
eg x in x = y + 5;
*/
typedef struct
{
	char name[MAX_LITERAL_NAME];
}var_ref_expr_data_t;

typedef enum
{
	expr_unary_op,
	expr_binary_op,
	expr_int_literal,
	expr_assign,
	expr_var_ref

}expression_kind;

typedef struct ast_expression
{
	token_range_t tokens;
	expression_kind kind;
	union
	{
		unary_op_expr_data_t unary_op;
		binary_op_expr_data_t binary_op;
		uint32_t const_val;
		assign_expr_data_t assignment;
		var_ref_expr_data_t var_reference;
	}data;
}ast_expression_t;

/* Statements */

typedef enum
{
	smnt_return,
	smnt_expr,
	smnt_var_decl
}statement_kind;

typedef struct ast_statement
{
	token_range_t tokens;
	statement_kind kind;
	ast_expression_t* expr;

	char decl_name[MAX_LITERAL_NAME]; //if kind == smnt_var_decl

	struct ast_statement* next;
}ast_statement_t;

/* Functions */

typedef struct ast_function
{
	token_range_t tokens;
	char name[32];

	//params
	//return type
	//static?

	ast_statement_t* statements;

}ast_function_decl_t;

typedef struct
{
	char path[256];
	ast_function_decl_t* function;
}ast_trans_unit_t;


void ast_print(ast_trans_unit_t* tl);

const char* ast_op_name(op_kind);

void ast_destory_translation_unit(ast_trans_unit_t* tl);