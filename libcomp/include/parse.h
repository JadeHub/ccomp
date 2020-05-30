#pragma once

#if defined(__cplusplus)
extern "C"
{
#endif

#include "token.h"
#include "ast.h"

ast_trans_unit_t* parse_translation_unit(token_t*);

#if defined(__cplusplus)
}
#endif