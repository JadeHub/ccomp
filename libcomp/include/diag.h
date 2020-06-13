#pragma once

#if defined(__cplusplus)
extern "C"
{
#endif

#include <stdint.h>
#include <stdarg.h>

#define ERR_SYNTAX 1
#define ERR_EXPECTED_TOK 2
#define ERR_FUNC_DUP_BODY 3			//Multiple definitions of the same function
#define ERR_FUNC_DIFF_PARAMS 4		//Multiple declarations with different param lists
#define ERR_DUP_VAR 5
#define ERR_UNKNOWN_VAR 6
#define ERR_UNKNOWN_FUNC 7
#define ERR_INVALID_PARAMS 8

struct token;

typedef void (*diag_cb)(struct token* toc, uint32_t err, const char* msg, void* data);

void diag_set_handler(diag_cb, void* data);
void diag_err(struct token* tok, uint32_t err, const char* format, ...);
const char* diag_pos_str(struct token*);

#if defined(__cplusplus)
}
#endif