#pragma once

#if defined(__cplusplus)
extern "C"
{
#endif

#include <stdint.h>

#define ERR_SYNTAX 1

struct token;

typedef void (*diag_cb)(struct token* toc, uint32_t err, const char* msg);

#if defined(__cplusplus)
}
#endif