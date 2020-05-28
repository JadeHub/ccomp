#pragma once

#if defined(__cplusplus)
extern "C"
{
#endif

#include "token.h"
#include "ast.h"
#include "diag.h"

ast_trans_unit_t* parse_translation_unit(token_t*, diag_cb);

#if defined(__cplusplus)
}
#endif