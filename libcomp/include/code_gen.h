#pragma once

#include "diag.h"
#include "sema.h"

typedef void (*write_asm_cb)(const char* line, void* data);

void code_gen(valid_trans_unit_t*, write_asm_cb, void* data);
