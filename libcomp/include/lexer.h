#pragma once

#if defined(__cplusplus)
extern "C"
{
#endif

#include "token.h"

void lex_init();

bool lex_next_tok(source_range_t* src, const char* pos, token_t* result);

token_t* lex_source(source_range_t*); 

#if defined(__cplusplus)
}
#endif