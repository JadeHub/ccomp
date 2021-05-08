#pragma once

#include "token.h"

void pre_proc_init();
void pre_proc_deinit();
token_range_t pre_proc_file(const char* src_dir, token_range_t* range);