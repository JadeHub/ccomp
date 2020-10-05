#include "pp.h"
#include "diag.h"
#include "source.h"
#include "lexer.h"

#include <libj/include/hash_table.h>

#include <stdio.h>

/*
Map of identifier string to token_range_t*
*/

static hash_table_t* _defs;

static inline void _diag_expected(token_t* tok, tok_kind kind)
{
	diag_err(tok, ERR_SYNTAX, "syntax error: expected '%s' before '%s'",
		tok_kind_spelling(kind), tok_kind_spelling(tok->kind));
}

static inline bool _is_pp_tok(token_t* tok)
{
	return tok->kind >= tok_pp_include &&
			tok->kind <= tok_pp_endif;
}

static token_t* _find_eol(token_t* from)
{
	token_t* tok = from;

	while (tok)
	{
		if (tok->flags & TF_START_LINE)
			return tok->prev;
		tok = tok->next;
	}
	return tok;
}

static void _tok_replace(token_range_t* remove, token_t* insert)
{
	token_t* end = tok_find_next(insert, tok_eof);

	if (remove->start->prev)
		remove->start->prev->next = insert;

}

static token_range_t* _process_define(token_t* tok)
{
	token_t* identifier = tok->next;
	const char* name = (const char*)identifier->data;

	if (identifier->next->kind == tok_l_paren && !(identifier->next->flags & TF_LEADING_SPACE))
	{
		//macro...
		return NULL;
	}

	token_range_t* range = (token_range_t*)malloc(sizeof(token_range_t));
	range->start = identifier->next;
	range->end = _find_eol(range->start);
	
	sht_insert(_defs, name, range);

	return range;
}

static token_t* _process_include(token_t* tok, bool local_inc)
{
	//todo resolve path

	source_range_t* sr = src_load_file((const char*)tok->data);
	if (!src_is_valid_range(sr))
	{
		diag_err(tok, ERR_UNKNOWN_SRC_FILE, "unknown file '%s'", (const char*)tok->data);
		return NULL;
	}

	token_t* toks = lex_source(sr);
	if (!toks)
		return NULL;
	return pre_proc_file(toks);
}

token_t* pre_proc_file(token_t* toks)
{
	token_t* result = toks;
	token_t* tok = toks;

	while (tok)
	{
		if (_is_pp_tok(tok))
		{
			switch (tok->kind)
			{
			case tok_pp_include:
			{
				token_t* path_tok = tok->next;
				if (path_tok->kind != tok_string_literal)
				{
					diag_err(tok, ERR_SYNTAX, "expected '<path>' or '\"path\"' after #include");
					return NULL;
				}
				const char* path = (const char*)path_tok->data;

				token_t* following_tok = path_tok->next;

				if (!(following_tok->flags & TF_START_LINE))
				{
					diag_err(following_tok, ERR_SYNTAX,
						"unexpected token '%s' after #include directive",
						tok_kind_spelling(following_tok->kind));
					return NULL;
				}

				token_t* inc_start = _process_include(path_tok, *path_tok->loc == '\"');
				if (!inc_start)
					return NULL;
				token_t* inc_end = tok_find_next(inc_start, tok_eof)->prev;

				//following_tok should come after inc_tok_end
				following_tok->prev = inc_end;
				inc_end->next = following_tok;

				if (tok == toks)
				{
					//#include was the first token
					result = inc_start;
				}
				else
				{
					tok->prev->next = inc_start;
					inc_start->prev = tok->prev;
				}
				tok = inc_end;
				break;
			}
			case tok_pp_define:
			{
				token_range_t* define = _process_define(tok);

				
				
				break;
			}
			}
		}

		tok = tok->next;
	}
	return result;
}

void pre_proc_init()
{
	_defs = sht_create(128);
}

void pre_proc_deinit()
{
	ht_destroy(_defs);
}