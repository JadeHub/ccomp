#pragma once

#include "token.h"
#include "ast.h"

typedef void (*diag_cb)(token_t* toc, uint32_t err, const char* msg);

ast_trans_unit_t* parse_translation_unit(token_t*, diag_cb);
