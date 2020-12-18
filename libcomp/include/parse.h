#pragma once

#include "token.h"
#include "ast.h"

void parse_init(token_t* tok);
ast_trans_unit_t* parse_translation_unit();

