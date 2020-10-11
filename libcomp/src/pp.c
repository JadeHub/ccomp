#include "pp.h"
#include "diag.h"
#include "source.h"
#include "lexer.h"
#include "ast.h"

#include <libj/include/hash_table.h>

#include <string.h>

static token_t* _process_token(token_t* tok);

typedef struct
{	
	/*
	Map of identifier string to token_range_t*
	*/
	hash_table_t* defs;
	token_t* first;
	token_t* last;
}pp_context_t;

static pp_context_t* _context = NULL;

static inline void _emit_token(token_t* tok)
{
	tok->prev = tok->next = NULL;
	if (_context->first == NULL)
	{
		_context->first = _context->last = tok;
	}
	else
	{
		_context->last->next = tok;
		tok->prev = _context->last;
		_context->last = tok;
	}
}

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

static void _emit_range(token_range_t* range)
{
	token_t* t = range->start;

	while (t && t != range->end)
	{
		token_t* next = t->next;
		_emit_token(t);
		t = next;
	}
}

static void _tok_replace(token_range_t* remove, token_t* insert)
{
	token_t* end = tok_find_next(insert, tok_eof);

	if (remove->start->prev)
		remove->start->prev->next = insert;
}

static token_range_t _expand_identifier(token_t* tok)
{
	char name[MAX_LITERAL_NAME + 1];
	tok_spelling_cpy(tok, name, MAX_LITERAL_NAME + 1);

	token_range_t* range = (token_range_t*)sht_lookup(_context->defs, name);
	if (range)
		return *range;

	token_range_t r = { tok, tok->next };
	return r;
}

static bool _process_define(token_t* tok, token_t* end)
{
	token_t* identifier = tok->next;

	char name[MAX_LITERAL_NAME + 1];
	tok_spelling_cpy(identifier, name, MAX_LITERAL_NAME + 1);

	if (identifier->next->kind == tok_l_paren && !(identifier->next->flags & TF_LEADING_SPACE))
	{
		//macro...
		return false;
	}

	token_range_t* range = (token_range_t*)malloc(sizeof(token_range_t));
	range->start = identifier->next;
	range->end = end->next;

	sht_insert(_context->defs, name, range);

	tok_destory(tok);
	tok_destory(identifier);
	return true;
}

//Return next token to be processed
static token_t* _process_include(token_t* tok)
{
	tok = tok->next;
	token_t* end = _find_eol(tok)->next;
	token_range_t range = { tok, end };

	
	if (tok->kind == tok_identifier)
	{
		//#define INC <blah.h>
		//#include INC
		range = _expand_identifier(tok);
	}

	char path[1024];
	if (range.start->kind == tok_string_literal)
	{
		//#include "blah.h"
		strcpy(path, (const char*)range.start->data);
		tok = range.start->next;
		if(tok != range.end)
		{
			diag_err(tok, ERR_SYNTAX,
				"expected newline after #include directive");
			return NULL;
		}
	}
	else if (range.start->kind == tok_lesser)
	{
		//#include <blah.h>
		tok = range.start->next;

		if (tok->kind != tok_identifier)
		{
			diag_err(tok, ERR_SYNTAX, "expected '<path>' or '\"path\"' after #include");
			return NULL;
		}
		tok_spelling_cpy(tok, path, 1024);
		tok = tok->next;

		if (tok->kind == tok_fullstop)
		{
			strncat(path, ".", 1);
			tok = tok->next;
			if (tok->kind == tok_identifier)
			{
				char buff[128];
				tok_spelling_cpy(tok, buff, 128);
				strncat(path, buff, strlen(buff));
				tok = tok->next;
			}			
		}
		
		if (tok->kind != tok_greater)
		{
			diag_err(tok, ERR_SYNTAX, "expected '<path>' or '\"path\"' after #include");
			return NULL;
		}
	}

	source_range_t* sr = src_load_file(path);
	if (!src_is_valid_range(sr))
	{
		diag_err(tok, ERR_UNKNOWN_SRC_FILE, "unknown file '%s'", path);
		return NULL;
	}

	tok = lex_source(sr);
	if (!tok)
		return false;

	while (tok->kind != tok_eof)
	{
		tok = _process_token(tok);
	}
	
	return end;
}

static token_t* _process_token(token_t* tok)
{
	token_t* next = tok->next;

	if (_is_pp_tok(tok))
	{
		switch (tok->kind)
		{
		case tok_pp_include:
		{
			next = _process_include(tok);
			if(!next)
				return NULL;
			break;
		}
		case tok_pp_define:
		{
			token_t* end = _find_eol(tok->next);
			_process_define(tok, end);
			next = end->next;
			break;
		}
		case tok_pp_ifdef:
			break;
		}
	}
	else if (tok->kind == tok_identifier)
	{
		next = tok->next;
		token_range_t range = _expand_identifier(tok);
		_emit_range(&range);
	}
	else
	{
		_emit_token(tok);
	}
	return next;
}

token_t* pre_proc_file(token_t* toks)
{
	token_t* result = toks;
	token_t* tok = toks;

	while (tok)
	{
		tok = _process_token(tok);

	}
	return _context->first;
}

void pre_proc_init()
{
	_context = (pp_context_t*)malloc(sizeof(pp_context_t));
	_context->defs = sht_create(128);
	_context->first = _context->last = NULL;
}

void pre_proc_deinit()
{
	ht_destroy(_context->defs);
	free(_context);
	_context = NULL;
}