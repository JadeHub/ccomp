#pragma once

#include "source.h"

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
    tok_semi_colon,         // ;
    tok_minus,              // -
    tok_minusminus,         // --
    tok_tilda,              // ~
    tok_exclaim,            // !
    tok_plus,               // +
    tok_plusplus,           // ++
    tok_plusequal,          // +=
    tok_star,               // *
    tok_slash,              // /

    tok_amp,                // &
    tok_ampamp,             // &&
    tok_pipe,               // |
    tok_pipepipe,           // ||
    tok_equal,              // =
    tok_equalequal,         // ==
    tok_exclaimequal,       // !=
    tok_lesser,             // <
    tok_lesserlesser,       // <<
    tok_lesserequal,        // <=
    tok_greater,            // >
    tok_greatergreater,     // >>
    tok_greaterequal,       // >=

    tok_percent,            // %
    tok_caret,              // ^
    tok_colon,              // :
    tok_question,           // ?
    tok_comma,              // ,

    //keywords
    tok_return,
    tok_int,
    tok_if,
    tok_else,
    tok_for,
    tok_while,
    tok_do,
    tok_break,
    tok_continue,

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

typedef struct
{
    token_t* start;
    token_t* end;

}token_range_t;


const char* tok_kind_spelling(tok_kind);

void tok_printf(token_t* tok);

void tok_spelling_cpy(token_t* tok, char* dest, size_t len);

bool tok_spelling_cmp(token_t* tok, const char* str);
