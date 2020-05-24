#pragma once

#include "source_range.h"

#include <stdbool.h>

typedef enum
{
    tok_eof,
    tok_invalid,

    //punctuation
    tok_l_brace,            // {
    tok_r_brace,            // }
    tok_l_paren,            // (
    tok_r_paren,            // )
    tok_semi_colon,          // ;

    //keywords
    tok_return,
    tok_int,

    tok_identifier,         // main
    tok_num_literal         // 1234
}tok_kind;

typedef struct token
{
    const char* loc;
    size_t len;
    tok_kind kind;
    void* data;
    struct token* next;
    struct token* prev;
}token_t;

const char* tok_kind_name(tok_kind);

void tok_printf(token_t* tok);

void tok_spelling_cpy(token_t* tok, char* dest, size_t len);

bool tok_spelling_cmp(token_t* tok, const char* str);
