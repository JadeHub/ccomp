#pragma once

#include "diag.h"
#include "ast.h"

typedef void (*write_asm_cb)(const char* line);

void code_gen(ast_trans_unit_t*, write_asm_cb);
