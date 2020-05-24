#pragma once

#include "token.h"

void lex_init();

bool lex_next_tok(source_range_t* src, const char* pos, token_t* result);

token_t* lex_source(source_range_t*);