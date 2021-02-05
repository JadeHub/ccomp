#pragma once

#include "diag.h"
#include "sema.h"

typedef void (*write_asm_cb)(const char* line, bool lf, void* data);

void code_gen(valid_trans_unit_t*, write_asm_cb, void* data, bool annotate);
