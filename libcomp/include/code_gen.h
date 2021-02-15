#pragma once

#include "diag.h"
#include "sema.h"
#include "var_set.h"

typedef void (*write_asm_cb)(const char* line, bool lf, void* data);

void code_gen(valid_trans_unit_t*, write_asm_cb, void* data, bool annotate);


//internal
void gen_expression(ast_expression_t* expr);
var_set_t* gen_var_set();
void gen_make_label_name(char* name);
void gen_asm(const char* format, ...);
void gen_annotate(const char* format, ...);
void gen_annotate_start(const char* format, ...);
void gen_annotate_end();