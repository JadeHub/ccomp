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

/*
conditional expression
x ? y : z;
*/
typedef struct
{
	struct ast_expression* cond;
	struct ast_expression* true_branch;
	struct ast_expression* false_branch;
}var_cond_expr_data_t;

typedef enum
{
	expr_unary_op,
	expr_binary_op,
	expr_int_literal,
	expr_assign,
	expr_var_ref,
	expr_condition,
	expr_null
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
		var_cond_expr_data_t condition;
	}data;
}ast_expression_t;

/* Declaration */
typedef struct
{
	token_range_t tokens;
	char decl_name[MAX_LITERAL_NAME]; //if kind == smnt_var_decl
	ast_expression_t* expr;
}ast_var_decl_t;

/* Statement */
typedef enum
{
	smnt_return,
	smnt_expr,
	smnt_if,
	smnt_compound,

	smnt_for,
	smnt_for_decl,
	smnt_while,
	smnt_do,
	smnt_break,
	smnt_continue,
}statement_kind;

typedef struct
{
	ast_expression_t* condition;
	struct ast_statement* true_branch;
	struct ast_statement* false_branch;

}ast_if_smnt_data_t;

typedef struct
{
	struct ast_block_item* blocks;
}ast_compound_smnt_data_t;

typedef struct
{
	ast_expression_t* condition;
	struct ast_statement* statement;
}ast_while_smnt_data_t;

/*
for(i=0;i<10;i++) 
{
}
*/
typedef struct
{
	ast_var_decl_t* init_decl; //for(int i = 0;...
	ast_expression_t* init; //for(i = 0;...
	ast_expression_t* condition;
	ast_expression_t* post;
	struct ast_statement* statement;
}ast_for_smnt_data_t;

typedef struct ast_statement
{
	token_range_t tokens;
	statement_kind kind;

	union
	{
		ast_expression_t* expr; //return and expr types
		ast_if_smnt_data_t if_smnt;
		ast_compound_smnt_data_t compound;
		ast_while_smnt_data_t while_smnt;
		ast_for_smnt_data_t for_smnt;
	}data;
}ast_statement_t;

/* Block item */

typedef enum
{
	blk_smnt,
	blk_var_def

}ast_block_item_type;

typedef struct ast_block_item
{
	token_range_t tokens;
	ast_block_item_type kind;

	union
	{
		ast_statement_t* smnt;
		ast_var_decl_t* var_decl;
	};
	struct ast_block_item* next;
}ast_block_item_t;

/* Functions */

typedef struct ast_function
{
	token_range_t tokens;
	char name[32];

	//params
	//return type
	//static?

	//ast_statement_t* statements;
	ast_block_item_t* blocks;

}ast_function_decl_t;

typedef struct
{
	token_range_t tokens;
	char path[256];
	ast_function_decl_t* function;
}ast_trans_unit_t;

void ast_print(ast_trans_unit_t* tl);

const char* ast_op_name(op_kind);

void ast_destory_translation_unit(ast_trans_unit_t* tl);