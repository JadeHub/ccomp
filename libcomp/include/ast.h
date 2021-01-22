#pragma once

#include <stdint.h>
#include "token.h"
#include "int_val.h"
#include "ast_internal.h"
#include "ast_op_kinds.h"
#include "ast_type_spec.h"

struct ast_expression;


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
variable reference data
eg x or y in x = y + 5;
*/
typedef struct
{
	char name[MAX_LITERAL_NAME];
	struct ast_declaration* decl; //set in sema
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

typedef struct ast_func_call_param
{
	struct ast_expression* expr;

	struct ast_type_spec* expr_type; //set during analysis

	struct ast_func_call_param* next, * prev;
}ast_func_call_param_t;

/*
function call expression data
*/
typedef struct
{
	struct ast_expression* target;

	ast_func_call_param_t* first_param;
	ast_func_call_param_t* last_param;

	uint32_t param_count;

	struct {
		struct ast_type_spec* target_type;
	}sema; //set during validation

}ast_expr_func_call_t;

typedef struct
{
	enum {sizeof_type, sizeof_expr} kind;
	union
	{
		struct ast_expression* expr;
		struct ast_type_ref* type_ref;
	}data;
}ast_sizeof_call_t;

typedef struct
{
	int_val_t val;

	//Type to ultimatly be set to the smallest size required to hold 'value'
	struct ast_type_spec* type;
}ast_int_literal_t;

typedef struct
{
	//string literal value extracted during lex stage
	const char* value;

	//label name used during code generation. Set during semantic analysis
	const char* label;
}ast_string_literal_t;

typedef struct
{
	struct ast_expression* expr;
	struct ast_type_ref* type_ref;
}ast_expr_cast_data_t;

typedef enum
{
	expr_postfix_op,
	expr_unary_op,
	expr_binary_op,
	expr_int_literal,
	expr_str_literal,
	expr_identifier,
	expr_condition,
	expr_func_call,
	expr_sizeof,
	expr_cast,
	expr_null
}expression_kind;

typedef struct ast_expression
{
	token_range_t tokens;
	expression_kind kind;
	union
	{
		ast_expr_unary_op_t unary_op; //expr_unary_op & expr_postfix_op
		ast_expr_binary_op_t binary_op;
		ast_int_literal_t int_literal;
		ast_string_literal_t str_literal;
		ast_expr_identifier_t identifier;
		ast_cond_expr_data_t condition;
		ast_expr_func_call_t func_call;
		ast_sizeof_call_t sizeof_call;
		ast_expr_cast_data_t cast;
	}data;
}ast_expression_t;

/* Type */
typedef enum
{
	type_void,
	type_int8,
	type_int16,
	type_int32,
	type_int64,

	type_uint8,
	type_uint16,
	type_uint32,
	type_uint64,

	/*type_float,
	type_double,*/
	type_user,
	type_ptr,
	type_func_sig,
	type_alias //typedef
}type_kind;

typedef struct ast_struct_member
{
	token_range_t tokens;
	struct ast_type_ref* type_ref;
	char name[MAX_LITERAL_NAME]; //name is optional
	uint32_t bit_size; //eg, the 1 in 'int p : 1'

	uint32_t offset; //alligned position within struct

	struct ast_struct_member* next;
}ast_struct_member_t;

typedef struct ast_enum_member
{
	token_range_t tokens;
	char name[MAX_LITERAL_NAME]; //name is optional
	ast_expression_t* value;
	struct ast_enum_member* next;
}ast_enum_member_t;

typedef enum
{
	user_type_struct,
	user_type_union,
	user_type_enum
}user_type_kind;

static inline const char* user_type_kind_name(user_type_kind k)
{
	if (k == user_type_struct)
		return "struct";
	else if (k == user_type_union)
		return "union";
	return "enum";
}

typedef struct ast_user_type_spec
{
	token_range_t tokens;
	char name[MAX_LITERAL_NAME]; //name is optional
	user_type_kind kind;

	union
	{
		ast_struct_member_t* struct_members;
		ast_enum_member_t* enum_members;
	}data;
}ast_user_type_spec_t;

typedef struct ast_func_sig_type_spec
{
	struct ast_type_spec* ret_type;
	struct ast_func_params* params;

}ast_func_sig_type_spec_t;

typedef struct ast_type_spec
{
	type_kind kind;
	uint32_t size;

	union
	{
		ast_user_type_spec_t* user_type_spec;
		ast_func_sig_type_spec_t* func_sig_spec;
		const char* alias;
		/*
		type pointed to if kind is type_ptr
		*/
		struct ast_type_spec* ptr_type;
	}data;

}ast_type_spec_t;

//Storage class flags
#define TF_SC_EXTERN		1
#define TF_SC_STATIC		2
#define TF_SC_TYPEDEF		4

//Type qualifier flags
#define TF_QUAL_CONST		8

typedef struct ast_type_ref
{
	token_range_t tokens;
	ast_type_spec_t* spec;
	uint32_t flags;
}ast_type_ref_t;

/* Declaration */
typedef struct ast_var_decl
{
	ast_expression_t* init_expr;

}ast_var_decl_t;

typedef struct ast_func_param_decl
{
	struct ast_declaration* decl;

	struct ast_func_param_decl* next, * prev;
}ast_func_param_decl_t;

typedef struct ast_func_params
{
	ast_func_param_decl_t* first_param;
	ast_func_param_decl_t* last_param;
	uint32_t param_count;
	bool ellipse_param;
}ast_func_params_t;

typedef struct ast_func_decl
{
	uint32_t required_stack_size;

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

	//optional name
	char name[MAX_LITERAL_NAME];

	//variable or function return type
	ast_type_ref_t* type_ref;

	//optional array size expression ie [....]
	ast_expression_t* array_sz;

	union
	{
		ast_var_decl_t var;
		ast_function_decl_t func;
	}data;

	struct ast_declaration* next;
}ast_declaration_t;

typedef struct
{
	ast_declaration_t* first, * last;
}ast_decl_list_t;

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
	smnt_switch,
	smnt_label,
	smnt_goto
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
	ast_decl_list_t decls; //for(int i = 0;...

	ast_expression_t* init; //for(i = 0;...
	ast_expression_t* condition; //for(...;i<10;...
	ast_expression_t* post; //for(...;...;i++)
	struct ast_statement* statement;
}ast_for_smnt_data_t;

typedef struct ast_switch_case_data
{
	ast_expression_t* const_expr;
	struct ast_statement* statement;
	struct ast_switch_case_data* next;
}ast_switch_case_data_t;

typedef struct
{
	ast_expression_t* expr;
	ast_switch_case_data_t* default_case;
	ast_switch_case_data_t* cases;
	uint32_t case_count;
}ast_switch_smnt_data_t;

typedef struct
{
	char label[MAX_LITERAL_NAME];
	struct ast_statement* smnt;
}ast_label_smnt_data_t;

typedef struct
{
	char label[MAX_LITERAL_NAME];
}ast_goto_smnt_data_t;

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
		ast_switch_smnt_data_t switch_smnt;
		ast_label_smnt_data_t label_smnt;
		ast_goto_smnt_data_t goto_smnt;
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
		ast_decl_list_t decls;
	}data;
	struct ast_block_item* next;
}ast_block_item_t;

typedef struct
{
	token_range_t tokens;
	char path[256];
	ast_decl_list_t decls;

}ast_trans_unit_t;

void ast_print(ast_trans_unit_t* tl);

bool ast_is_assignment_op(op_kind op);
const char* ast_op_name(op_kind);
const char* ast_declaration_name(ast_declaration_t* decl);
const char* ast_declaration_type_name(ast_decl_type decl_type);
const char* ast_type_name(ast_type_spec_t* type);
const char* ast_type_ref_name(ast_type_ref_t* ref);
ast_struct_member_t* ast_find_struct_member(ast_user_type_spec_t* struct_spec, const char* name);
uint32_t ast_user_type_size(ast_user_type_spec_t*);
ast_type_spec_t* ast_make_ptr_type(ast_type_spec_t* ptr_type);
ast_type_spec_t* ast_make_func_ptr_type(ast_type_spec_t* ret_type, ast_func_params_t* params);
ast_type_spec_t* ast_make_func_sig_type(ast_type_spec_t* ret_type, ast_func_params_t* params);
ast_type_spec_t* ast_func_decl_return_type(ast_declaration_t* fn);
ast_func_params_t* ast_func_decl_params(ast_declaration_t* fn);

uint32_t ast_decl_type_size(ast_declaration_t* decl);
bool ast_type_is_fn_ptr(ast_type_spec_t* type);
bool ast_type_is_signed_int(ast_type_spec_t* type);
bool ast_type_is_int(ast_type_spec_t* type);
bool ast_type_is_enum(ast_type_spec_t* type);

void ast_destroy_decl_list(ast_decl_list_t decl_list);
void ast_destory_translation_unit(ast_trans_unit_t* tl);
void ast_destroy_statement(ast_statement_t*);
void ast_destroy_expression(ast_expression_t*);
void ast_destroy_expression_data(ast_expression_t*);
void ast_destroy_declaration(ast_declaration_t*);
void ast_destroy_type_spec(ast_type_spec_t*);
