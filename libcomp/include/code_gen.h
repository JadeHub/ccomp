#pragma once

//code generation

#include "diag.h"
#include "sema.h"
#include "var_set.h"

/*
callback used to output asm.

line - single line of assembly code
lf - follow with line feed if true
data - callback context data
*/
typedef void (*write_asm_cb)(const char* line, bool lf, void* data);

/*
generate assembly code for a given translation unit

tl - trasnlation unit generated from semantic alaysis phase
cb - callback for writing assembly output
data - context data passed to callback
annotate - generate code annotations if true
*/
void code_gen(valid_trans_unit_t* tl, write_asm_cb cb, void* data, bool annotate);

//internal
void gen_expression(ast_expression_t* expr);
var_set_t* gen_var_set();
void gen_make_label_name(char* name);
void gen_asm(const char* format, ...);
void gen_annotate(const char* format, ...);
void gen_annotate_start(const char* format, ...);
void gen_annotate_end();