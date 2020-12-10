#pragma once

#include "ast.h"
#include "int_val.h"

#include <stdbool.h>

typedef struct
{
	bool success;
	int_val_t val;

}int_eval_result_t;

bool sema_is_int_constant_expression(ast_expression_t* expr);
int_val_t sema_eval_constant_expr(ast_expression_t* expr);
bool process_sizeof_expr(ast_expression_t* expr);

ast_type_spec_t* int_val_smallest_size(int_val_t* val);
bool int_val_will_fit(int_val_t* val, ast_type_spec_t* type);