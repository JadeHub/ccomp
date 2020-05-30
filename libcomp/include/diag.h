#pragma once

#if defined(__cplusplus)
extern "C"
{
#endif

#include <stdint.h>
#include <stdarg.h>

#define ERR_SYNTAX 1

struct token;

typedef void (*diag_cb)(struct token* toc, uint32_t err, const char* msg);

void diag_set_handler(diag_cb);
void diag_err(struct token* tok, uint32_t err, const char* format, ...);

#if defined(__cplusplus)
}
#endif