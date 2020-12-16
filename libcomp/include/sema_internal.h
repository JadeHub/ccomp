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
	ast_type_spec_t* result_type;
}expr_result_t;

expr_result_t sema_process_expression(ast_expression_t* expr);

identfier_map_t* sema_id_map();
ast_type_spec_t* sema_resolve_type(ast_type_spec_t* typeref);
bool sema_can_convert_type(ast_type_spec_t* target, ast_type_spec_t* type);



bool sema_is_int_constant_expression(ast_expression_t* expr);
int_val_t sema_eval_constant_expr(ast_expression_t* expr);
bool process_sizeof_expr(ast_expression_t* expr);

ast_type_spec_t* int_val_smallest_size(int_val_t* val);
bool int_val_will_fit(int_val_t* val, ast_type_spec_t* type);