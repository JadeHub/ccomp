#pragma once

#include "ast.h"
#include "int_val.h"
#include "id_map.h"

#include <stdbool.h>

/*
Result of analysing an expression
*/
typedef struct
{
	bool failure;
	ast_type_spec_t* result_type; //can be replaced with ast_expression_t::sema::result_type
	ast_struct_member_t* member; //result was a struct member access (this is used to implement the rule which prevents taking the address of a bit field member
}expr_result_t;

expr_result_t sema_process_expression(ast_expression_t* expr);
expr_result_t sema_process_int_literal(ast_expression_t* expr);

/*
Result of analysing a definition
*/
typedef enum
{
	proc_decl_new_def,
	proc_decl_ignore,
	proc_decl_error
}proc_decl_result;

proc_decl_result sema_process_global_variable_declaration(ast_declaration_t* decl);

expr_result_t sema_report_type_conversion_error(ast_expression_t* expr, ast_type_spec_t* expected, ast_type_spec_t* actual, const char* format, ...);

/*
Details of function currently being analysed
*/
typedef struct
{
	ast_declaration_t* decl;

	//set of goto statements found in the function
	hash_table_t* goto_smnts;

	//set of strings used as labels
	hash_table_t* labels;
}func_context_t;

func_context_t* sema_get_cur_fn_ctx();

identfier_map_t* sema_id_map();
bool sema_resolve_type_ref(ast_type_ref_t* ref);
ast_type_spec_t* sema_resolve_type(ast_type_spec_t* spec, token_t* start);
bool sema_resolve_function_sig_types(ast_func_sig_type_spec_t* fsig, token_t* start);
bool sema_can_convert_type(ast_type_spec_t* target, ast_type_spec_t* type);
bool sema_is_same_type(ast_type_spec_t* lhs, ast_type_spec_t* rhs);
bool sema_is_same_func_sig(ast_func_sig_type_spec_t* lhs, ast_func_sig_type_spec_t* rhs);
bool sema_is_const_int_expr(ast_expression_t* expr);

ast_expression_t* sema_fold_const_int_expr(ast_expression_t* expr);
bool process_sizeof_expr(ast_expression_t* expr);
ast_type_spec_t* int_val_smallest_size(int_val_t* val);
bool int_val_will_fit(int_val_t* val, ast_type_spec_t* type);
bool sema_can_perform_assignment(ast_type_spec_t* target, ast_expression_t* source);


//decl
proc_decl_result sema_process_function_decl(ast_declaration_t* decl);
bool sema_process_type_decl(ast_declaration_t* decl);
bool sema_process_variable_declaration(ast_declaration_t* decl);

/*
Returns true if the expression in a pointer cast of a const int expression
ie, something like (void*)0

todo - addresses of globals should also be considered const
*/
bool sema_is_const_pointer(ast_expression_t* expr);