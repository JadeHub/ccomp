#pragma once

#include "ast.h"
#include "int_val.h"
#include "id_map.h"

#include <stdbool.h>

typedef struct
{
	uint32_t err;
	char* message;
}sema_diag_t;

/*
Result of analysing an expression
*/
typedef struct
{
	bool failure;
	ast_type_spec_t* result_type; //can be replaced with ast_expression_t::sema::result_type
	bool array;
}expr_result_t;

expr_result_t sema_process_expression(ast_expression_t* expr);
expr_result_t sema_process_int_literal(ast_expression_t* expr);

identfier_map_t* sema_id_map();
bool sema_resolve_type_ref(ast_type_ref_t* ref);
bool sema_can_convert_type(ast_type_spec_t* target, ast_type_spec_t* type);
bool sema_is_same_type(ast_type_spec_t* lhs, ast_type_spec_t* rhs);
bool sema_is_const_int_expr(ast_expression_t* expr);
int_val_t sema_fold_const_int_expr(ast_expression_t* expr);
bool process_sizeof_expr(ast_expression_t* expr);
ast_type_spec_t* int_val_smallest_size(int_val_t* val);
bool int_val_will_fit(int_val_t* val, ast_type_spec_t* type);