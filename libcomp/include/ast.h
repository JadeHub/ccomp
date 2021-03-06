#pragma once

//abstract syntax tree data structures

#include <stdint.h>
#include "token.h"
#include "int_val.h"
#include "ast_op_kinds.h"

#define MAX_LITERAL_NAME 32
#define MAX_LBL_LEN 16

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
}ast_cond_expr_t;

/*
functional call parameter
*/
typedef struct ast_func_call_param
{
	struct ast_expression* expr;
	struct ast_func_call_param* next, * prev;
}ast_func_call_param_t;

/*
function call expression data
*/
typedef struct
{
	/*
	target is either a function identifier or an expression which resolves to a function pointer
	*/
	struct ast_expression* target;

	/*
	parameter list
	*/
	ast_func_call_param_t* first_param;
	ast_func_call_param_t* last_param;

	uint32_t param_count;

	struct {
		/*
		result type of target expression above
		*/
		struct ast_type_spec* target_type;
	}sema;

}ast_expr_func_call_t;

/*
sizeof call data
*/
typedef struct
{
	/*
	kind of sizeof call
	*/
	enum
	{
		sizeof_type,	//eg sizeof(int)
		sizeof_expr		//eg sizeof(var)
	} kind;
	
	union
	{
		struct ast_expression* expr;
		struct ast_type_ref* type_ref;
	}data;
}ast_sizeof_call_t;

/*
integer literal value
*/
typedef struct
{
	int_val_t val;
}ast_int_literal_t;

/*
* string literal data
*/
typedef struct
{
	/*
	string literal value extracted during lex stage
	*/
	const char* value;

	struct {
		/*
		label name used during code generation
		*/
		const char* label;
	} sema;
}ast_string_literal_t;

/*
call expression data
*/
typedef struct
{
	/*
	expression to cast
	*/
	struct ast_expression* expr;

	/*
	result type
	*/
	struct ast_type_ref* type_ref;
}ast_expr_cast_t;

/*
* New
*/
typedef struct ast_compont_init_item
{
	struct ast_expression* expr;
	struct ast_compont_init_item* next;

	struct
	{
		struct ast_struct_member* member;
	}sema;
}ast_compound_init_item_t;

typedef struct
{
	ast_compound_init_item_t* item_list;

}ast_expr_compound_init_t;

typedef enum
{
	expr_unary_op,
	expr_binary_op,
	expr_int_literal,
	expr_str_literal,
	expr_identifier,
	expr_condition,
	expr_func_call,
	expr_sizeof,
	expr_cast,
	expr_struct_init,
	expr_compound_init,
	expr_null
}expression_kind;

static inline const char* ast_expr_kind_name(expression_kind k)
{
	switch (k)
	{
	case expr_unary_op:
		return "unary op";
	case expr_binary_op:
		return "binary op";
	case expr_int_literal:
		return "int literal";
	case expr_str_literal:
		return "string literal";
	case expr_identifier:
		return "identifier";
	case expr_condition:
		return "condition";
	case expr_func_call:
		return "function call";
	case expr_sizeof:
		return "sizeof";
	case expr_cast:
		return "cast";
	case expr_compound_init:
		return "compound initialiser";
	case expr_null:
		return "null";
	}
	return "";
}

/*
expression data
*/
typedef struct ast_expression
{
	token_range_t tokens;
	expression_kind kind;
	union
	{
		ast_expr_unary_op_t unary_op;
		ast_expr_binary_op_t binary_op;
		ast_int_literal_t int_literal;
		ast_string_literal_t str_literal;
		ast_expr_identifier_t identifier;
		ast_cond_expr_t condition;
		ast_expr_func_call_t func_call;
		ast_sizeof_call_t sizeof_call;
		ast_expr_cast_t cast;
		ast_expr_compound_init_t compound_init;
	}data;

	struct {
		struct {
			
			/*
			* expression result type
			*/
			struct ast_type_spec* type;
		}result;
	}sema;

}ast_expression_t;

/*
expression list item
*/
typedef struct ast_expression_list
{
	ast_expression_t* expr;
	struct ast_expression_list* next;
}ast_expression_list_t;

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
	type_array,
	type_func_sig,
	type_alias //typedef
}type_kind;

/*
member of a user defined struct or union
*/
typedef struct ast_struct_member
{
	/*
	declaration - includes name, type
	*/
	struct ast_declaration* decl;

	struct
	{
		/*
		If the member is a bit field these values will be set
		*/
		struct
		{
			size_t size;	//size in bits
			size_t offset;	//offset from start of member in bits
		}bit_field;

		size_t offset; //alligned position within struct - todo
	}sema;

	struct ast_struct_member* next;
}ast_struct_member_t;

/*
member of a user defined enumeration
*/
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

/*
returns the textual name for user type kind
*/
static inline const char* ast_user_type_kind_name(user_type_kind k)
{
	if (k == user_type_struct)
		return "struct";
	else if (k == user_type_union)
		return "union";
	return "enum";
}

/*
user type specification - struct, union or enum
*/
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

/*
function signature specification
*/
typedef struct ast_func_sig_type_spec
{
	/*
	type returned by function
	*/
	struct ast_type_spec* ret_type;

	/*
	*parameter list data
	*/
	struct ast_func_params* params;

}ast_func_sig_type_spec_t;

typedef struct
{
	//todo use ast_type_ref_t*
	struct ast_type_spec* element_type;
	ast_expression_t* size_expr;

	struct
	{
		size_t array_sz;
	}sema;

}ast_array_spec_t;

/*
type specification
either a built in type, a user defined type, a function pointer, a pointer to one of these or a type alias (typedef)
*/
typedef struct ast_type_spec
{
	type_kind kind;
	size_t size;

	union
	{
		ast_user_type_spec_t* user_type_spec;
		ast_func_sig_type_spec_t* func_sig_spec;
		const char* alias;
		/*
		type pointed to if kind is type_ptr
		*/
		struct ast_type_spec* ptr_type;

		ast_array_spec_t* array_spec;
	}data;
}ast_type_spec_t;

//Storage class flags
#define TF_SC_EXTERN		1
#define TF_SC_STATIC		2
#define TF_SC_TYPEDEF		4
#define TF_SC_INLINE		8

//Type qualifier flags
#define TF_QUAL_CONST		16

/*
a reference to a type specification including type qual and storage class information
*/
typedef struct ast_type_ref
{
	token_range_t tokens;
	ast_type_spec_t* spec;
	uint32_t flags;
}ast_type_ref_t;

/*
Variable declaration data
*/
typedef struct ast_var_decl
{
	/*
	initialisation expression
	*/
	ast_expression_t* init_expr;


	/*
	bit size expression for struct members
	*/
	ast_expression_t* bit_sz;
}ast_var_decl_data_t;

/*
Function parameter declaration
*/
typedef struct ast_func_param_decl
{
	struct ast_declaration* decl;

	struct ast_func_param_decl* next, * prev;
}ast_func_param_decl_t;

/*
Function delcaration parameter data
*/
typedef struct ast_func_params
{
	/*
	parameter list
	*/
	ast_func_param_decl_t* first_param;
	ast_func_param_decl_t* last_param;
	uint32_t param_count;

	/*
	true if the parameter list ended with '...'
	*/
	bool ellipse_param;
}ast_func_params_t;

/*
Function delcaration data
*/
typedef struct
{
	/*
	If this declration is actually a definition it will have a list of blocks
	*/
	struct ast_block_item* blocks;

	struct
	{
		size_t required_stack_size;
	}sema;
}ast_function_decl_data_t;

typedef enum
{
	decl_var,
	decl_func,
	decl_type
}ast_decl_kind;

static inline const char* ast_decl_kind_name(ast_decl_kind k)
{
	if (k == decl_var)
		return "variable";
	else if (k == decl_func)
		return "function";
	return "user type";
}

/*
A declaration
*/
typedef struct ast_declaration
{
	token_range_t tokens;
	ast_decl_kind kind;

	//optional name
	char name[MAX_LITERAL_NAME];

	ast_type_ref_t* type_ref;

	union
	{
		ast_var_decl_data_t var;
		ast_function_decl_data_t func;
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
	smnt_case,
	smnt_label,
	smnt_goto
}statement_kind;

/*
if statement data
*/
typedef struct
{
	ast_expression_t* condition;
	struct ast_statement* true_branch;
	struct ast_statement* false_branch;
}ast_if_smnt_data_t;

/*
compound statement data (block list)
*/
typedef struct
{
	struct ast_block_item* blocks;
}ast_compound_smnt_data_t;

/*
while statement data
*/
typedef struct
{
	ast_expression_t* condition;
	struct ast_statement* statement;
}ast_while_smnt_data_t;

/*
for loop statement data
*/
typedef struct
{
	ast_decl_list_t decls; //for(int i = 0;...;...)

	ast_expression_t* init; //for(i = 0;...;...)
	ast_expression_t* condition; //for(...;i<10;...)
	ast_expression_t* post; //for(...;...;i++)
	struct ast_statement* statement;
}ast_for_smnt_data_t;

/*
switch statement case data
*/
typedef struct
{
	ast_expression_t* expr;
	struct ast_statement* smnt;
	bool dflt;

	struct
	{
		char lbl[MAX_LBL_LEN];
	}sema;
}ast_case_smnt_data_t;

/*
switch statement data
*/
typedef struct
{
	ast_expression_t* expr;

	struct ast_statement* smnt; //most likely a compound statement containing case statements
	
	struct
	{
		ast_case_smnt_data_t* dflt_case;
		size_t case_count;
		bool last_smnt_was_case;

		ast_case_smnt_data_t** case_smnts;
	}sema;

	/*ast_switch_case_data_t* default_case;
	ast_switch_case_data_t* cases;
	uint32_t case_count;*/
}ast_switch_smnt_data_t;

/*
label statement data
*/
typedef struct
{
	/*
	label name
	*/
	char label[MAX_LITERAL_NAME];

	/*
	labels can optionally include a statement, ie 'label: return 0;'
	*/
	struct ast_statement* smnt;
}ast_label_smnt_data_t;

/*
goto statement case data
*/
typedef struct
{
	char label[MAX_LITERAL_NAME];
}ast_goto_smnt_data_t;

/*
a statement
*/
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
		ast_case_smnt_data_t case_smnt;
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

/*
a block item - either a declaration or a statement
*/
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

/*
translation unit
*/
typedef struct
{
	token_range_t tokens;
	char path[256];
	/*
	list of declrations - each either a global var, type or function
	*/
	ast_decl_list_t decls;
}ast_trans_unit_t;

/*
returns true if the operation is a form of assignment (=, +=, etc)
*/
bool ast_is_assignment_op(op_kind op);

/*
returns a textual name for an operation kind
*/
const char* ast_op_name(op_kind);

/*
returns the declaration's name
*/
const char* ast_declaration_name(ast_declaration_t* decl);

/*
Describe the type of a declaration
*/
void ast_decl_type_describe(str_buff_t* sb, ast_declaration_t* decl);

void ast_type_spec_desc(str_buff_t* sb, ast_type_spec_t* spec);

/*
returns the type name
*/
const char* ast_type_name(ast_type_spec_t* type);

/*
find a user type member by name
*/
ast_struct_member_t* ast_find_struct_member(ast_user_type_spec_t* struct_spec, const char* name);

/*
returns a type spec which represents an array of the given element type and size
*/
ast_type_spec_t* ast_make_array_type(ast_type_spec_t* element_type, ast_expression_t* size_expr);

/*
returns a type spec which represents a pointer to given type
*/
ast_type_spec_t* ast_make_ptr_type(ast_type_spec_t* type);

/*
returns a type spec which represents a pointer to a function with the given specification
*/
ast_type_spec_t* ast_make_func_ptr_type(ast_type_spec_t* ret_type, ast_func_params_t* params);

/*
returns a type spec which represents a function signature with the given specification
*/
ast_type_spec_t* ast_make_func_sig_type(ast_type_spec_t* ret_type, ast_func_params_t* params);

/*
returns a type spec which represents the type returned by the given function declaration
*/
ast_type_spec_t* ast_func_decl_return_type(ast_declaration_t* fn);

/*
returns the function parameter data from the given function declaration
*/
ast_func_params_t* ast_func_decl_params(ast_declaration_t* fn);

bool ast_expr_is_int_literal(ast_expression_t* expr, int64_t val);

/*
returns true if the given type spec is a pointer to a function signature
*/
bool ast_type_is_fn_ptr(ast_type_spec_t* type);

/*
returns true if the given type spec is an array
*/
bool ast_type_is_array(ast_type_spec_t* type);

/*
returns true if the given type spec is an alias
*/
bool ast_type_is_alias(ast_type_spec_t* type);

/*
returns true if the given type spec is a pointer
*/
bool ast_type_is_ptr(ast_type_spec_t* type);

/*
returns true if the given type spec is a signed integer type
*/
bool ast_type_is_signed_int(ast_type_spec_t* type);

/*
returns true if the given type spec is an integer type
*/
bool ast_type_is_int(ast_type_spec_t* type);

/*
returns true if the given type spec is an enum
*/
bool ast_type_is_enum(ast_type_spec_t* type);

/*
returns true if the given type spec is a struct or union
*/
bool ast_type_is_struct_union(ast_type_spec_t* type);

/*
returns true if the given type spec is a pointer to void (void*)
*/
bool ast_type_is_void_ptr(ast_type_spec_t* spec);

/*
returns true if the given struct member is a bit field
*/
bool ast_is_bit_field_member(ast_struct_member_t* member);

void ast_destroy_decl_list(ast_decl_list_t decl_list);
void ast_destory_translation_unit(ast_trans_unit_t* tl);
void ast_destroy_statement(ast_statement_t*);
void ast_destroy_expression(ast_expression_t*);
void ast_destroy_expression_data(ast_expression_t*);
void ast_destroy_declaration(ast_declaration_t*);
void ast_destroy_expression_list(ast_expression_list_t* list);
void ast_destroy_type_spec(ast_type_spec_t*);
