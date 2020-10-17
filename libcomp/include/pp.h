#pragma once

#include "token.h"

void pre_proc_init();
void pre_proc_deinit();
token_t* pre_proc_file(token_t* toks);