#pragma once

#include "tok_kind.h"
#include "source.h"

#include <libj/include/str_buff.h>

#include <stdbool.h>
#include <stdlib.h>


enum tok_flags
{
    TF_START_LINE = 0x01,
    TF_LEADING_SPACE = 0x02
};

typedef struct token
{
    const char* loc;
    size_t len;
    tok_kind kind;

    union
    {
        uint32_t integer;
        char* str;
    }data;

    uint32_t id;

    uint8_t flags;
    struct token* next;
    struct token* prev;
}token_t;

typedef struct
{
    token_t* start;
    token_t* end;
}token_range_t;

token_t* tok_create();
token_t* tok_find_next(token_t* start, tok_kind kind);
const char* tok_kind_spelling(tok_kind);
void tok_printf(token_t* tok);
void tok_print_range(token_range_t* range);
size_t tok_spelling_len(token_t* tok);
void tok_spelling_cpy(token_t* tok, char* dest, size_t len);
void tok_spelling_append(const char* src_loc, size_t src_len, str_buff_t* result);
size_t tok_range_len(token_range_t* range);
bool tok_range_empty(token_range_t* range);
void tok_destory(token_t* tok);
void tok_destroy_range(token_range_t* range);
bool tok_equals(token_t* lhs, token_t* rhs);
bool tok_range_equals(token_range_t* lhs, token_range_t* rhs);
token_t* tok_duplicate(token_t* tok);