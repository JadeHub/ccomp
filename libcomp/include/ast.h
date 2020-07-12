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

	op_member_access,

	//logical operators
	op_and,
	op_or,
	op_eq,
	op_neq,
	op_lessthan,
	op_lessthanequal,
	op_greaterthan,
	op_greaterthanequal,

	op_prefix_inc,
	op_prefix_dec,
	op_postfix_inc,
	op_postfix_dec

}op_kind;

/************************************************/
/*												*/
/* Expressions									*/
/*												*/
/************************************************/

/*
Unary operation expression data
eg !x
*/
typedef struct
{
	op_kind operation;
	struct ast_expression* expression;
}ast_expr_unary_op_t;

/*
Binary operation expression data
eg x + y
*/
typedef struct
{
	op_kind operation;
	struct ast_expression* lhs;
	struct ast_expression* rhs;
}ast_expr_binary_op_t;

/*
assignment expression data
eg x = 5
*/
typedef struct
{
	char name[MAX_LITERAL_NAME];
	struct ast_expression* target;
	struct ast_expression* expr;
}ast_expr_assign_t;

/*
variable reference data
eg x in x = y + 5;
*/
typedef struct
{
	char name[MAX_LITERAL_NAME];
}ast_expr_identifier_t;

/*
conditional expression
x ? y : z;
*/
typedef struct
{
	struct ast_expression* cond;
	struct ast_expression* true_branch;
	struct ast_expression* false_branch;
}ast_cond_expr_data_t;

/*
function call expression data
*/
typedef struct
{
	char name[MAX_LITERAL_NAME];
	struct ast_expression* target;
	struct ast_expression* params;
	uint32_t param_count;
}ast_expr_func_call_t;

typedef enum
{
	expr_postfix_op,
	expr_unary_op,
	expr_binary_op,
	expr_int_literal,
	expr_assign,
	expr_identifier,
	expr_condition,
	expr_func_call,
	expr_null
}expression_kind;

typedef struct ast_expression
{
	token_range_t tokens;
	expression_kind kind;
	union
	{
		ast_expr_unary_op_t unary_op;
		ast_expr_binary_op_t binary_op;
		uint32_t const_val;
		ast_expr_assign_t assignment;
		ast_expr_identifier_t var_reference;
		ast_cond_expr_data_t condition;
		ast_expr_func_call_t func_call;
	}data;
	struct ast_expression* next; //func call param list
}ast_expression_t;

/* Type */
typedef enum
{	
	type_void,
	/*type_char,
	type_short,*/
	type_int,
	/*type_long,
	type_float,
	type_double,*/
	type_struct
}type_kind;

typedef struct ast_struct_member
{
	token_range_t tokens;
	struct ast_type_spec* type;
	char name[MAX_LITERAL_NAME]; //name is optional
	uint32_t bit_size; //eg, the 1 in 'int p : 1'

	uint32_t offset;

	struct ast_struct_member* next;
}ast_struct_member_t;

typedef struct ast_struct_spec
{
	char name[MAX_LITERAL_NAME]; //name is optional
	enum
	{
		struct_struct,
		struct_union
	}kind;

	ast_struct_member_t* members;
}ast_struct_spec_t;

typedef struct ast_type_spec
{
	token_range_t tokens;
	type_kind kind;
	uint32_t size;	
	ast_struct_spec_t* struct_spec;
}ast_type_spec_t;

/* Declaration */

typedef struct ast_var_decl
{
	ast_type_spec_t* type;
	char name[MAX_LITERAL_NAME];
	ast_expression_t* expr;
}ast_var_decl_t;

/*typedef struct ast_function_param
{
	token_range_t tokens;
	char name[MAX_LITERAL_NAME];
	ast_type_spec_t* type;

	struct ast_function_param* next;
}ast_function_param_t;*/

typedef struct ast_func_decl
{
	char name[MAX_LITERAL_NAME];
	//ast_function_param_t* params;
	struct ast_declaration* params;
	uint32_t param_count;
	ast_type_spec_t* return_type;

	//static?

	//Definitions will have a list of blocks
	struct ast_block_item* blocks;

}ast_function_decl_t;

typedef enum
{
	decl_var,
	decl_func,
	decl_type
}ast_decl_type;

typedef struct ast_declaration
{
	token_range_t tokens;
	ast_decl_type kind;

	union
	{
		ast_var_decl_t var;
		ast_function_decl_t func;
		ast_type_spec_t type;
	}data;

	struct ast_declaration* next;
}ast_declaration_t;

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
	ast_declaration_t* init_decl; //for(int i = 0;...
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
	blk_decl
}ast_block_item_type;

typedef struct ast_block_item
{
	token_range_t tokens;
	ast_block_item_type kind;

	union
	{
		ast_statement_t* smnt;
		ast_declaration_t* decl;
	};
	struct ast_block_item* next;
}ast_block_item_t;

typedef struct
{
	token_range_t tokens;
	char path[256];
	ast_declaration_t* decls;
}ast_trans_unit_t;

void ast_print(ast_trans_unit_t* tl);

const char* ast_op_name(op_kind);
const char* ast_declaration_name(ast_declaration_t* decl);
const char* ast_type_name(ast_type_spec_t* type);

void ast_destory_translation_unit(ast_trans_unit_t* tl);
void ast_destroy_statement(ast_statement_t*);
void ast_destroy_expression(ast_expression_t*);
void ast_destroy_declaration(ast_declaration_t*);

typedef bool (*ast_fn_decl_visitor_cb)(ast_function_decl_t*);
typedef bool (*ast_block_item_visitor_cb)(ast_block_item_t*);
typedef bool (*ast_expr_visitor_cb)(ast_expression_t*);

bool ast_visit_block_items(ast_block_item_t*, ast_block_item_visitor_cb cb);
