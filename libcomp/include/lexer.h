#pragma once

#include "token.h"

void lex_init();
token_range_t lex_source(source_range_t*); 
