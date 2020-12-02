#pragma once

#include "ast.h"
#include "int_val.h"

#include <stdbool.h>

bool sema_is_constant_expression(ast_expression_t* expr);
int_val_t sema_eval_constant_expr(ast_expression_t* expr);