#include "pp.h"
#include "pp_internal.h"
#include "diag.h"
#include "source.h"
#include "lexer.h"
#include "ast.h"

#include <string.h>

static token_t* _process_token(token_t* tok);

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

static inline void* _diag_expected(token_t* tok, tok_kind kind)
{
	diag_err(tok, ERR_SYNTAX, "syntax error: expected '%s' before '%s'",
		tok_kind_spelling(kind), diag_tok_desc(tok));
	return NULL;
}

static inline bool _is_pp_tok(token_t* tok)
{
	return tok->kind >= tok_pp_null &&
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

static token_range_t* _find_def(token_t* ident)
{
	char name[MAX_LITERAL_NAME + 1];
	tok_spelling_cpy(ident, name, MAX_LITERAL_NAME + 1);

	return (token_range_t*)sht_lookup(_context->defs, name);
}

static token_range_t _expand_identifier(token_t* tok)
{
	token_range_t* range = _find_def(tok);
	if (range)
		return *range;

	token_range_t r = { tok, tok->next };
	return r;
}

static token_t* _process_undef(token_t* tok)
{
	token_t* identifier = tok->next;
	token_t* end = _find_eol(tok->next)->next;

	if (identifier->kind != tok_identifier)
		return _diag_expected(identifier, tok_identifier);

	char name[MAX_LITERAL_NAME + 1];
	tok_spelling_cpy(identifier, name, MAX_LITERAL_NAME + 1);

	token_range_t* range = (token_range_t*)sht_lookup(_context->defs, name);
	if (range)
	{
		sht_remove(_context->defs, name);
		tok_destroy_range(range);
		free(range);
	}
	return end;
}

static token_t* _process_define(token_t* tok)
{
	token_t* end = _find_eol(tok->next)->next;
	token_t* identifier = tok->next;

	if (identifier->kind != tok_identifier)
		return _diag_expected(identifier, tok_identifier);
	
	char name[MAX_LITERAL_NAME + 1];
	tok_spelling_cpy(identifier, name, MAX_LITERAL_NAME + 1);

	if (sht_lookup(_context->defs, name))
	{
		diag_err(tok, ERR_SYNTAX, "duplicate macro '%s'", name);
		return NULL;
	}

	if (identifier->next->kind == tok_l_paren && !(identifier->next->flags & TF_LEADING_SPACE))
	{
		//macro...
		return NULL;
	}

	token_range_t* range = (token_range_t*)malloc(sizeof(token_range_t));
	range->start = identifier->next;
	range->end = end;

	sht_insert(_context->defs, name, range);

	tok_destory(tok);
	tok_destory(identifier);
	return end;
}

//Return next token to be processed
static token_t* _process_include(token_t* tok)
{
	tok = tok->next;
	token_t* end = _find_eol(tok)->next;
	token_range_t range = { tok, end };
	include_kind inc_kind;
	
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
		inc_kind = include_local;
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
		inc_kind = include_system;
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
	else
	{
		diag_err(tok, ERR_SYNTAX, "expected '<path>' or '\"path\"' after #include");
		return NULL;
	}

	source_range_t* sr = src_load_header(path, inc_kind);
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

/*
Evaluate the condition in a #if or #elif pre proc expression
tok points to the start of the expression
returns the token after the expression
*/
static token_t* _eval_pp_expression(token_t* tok, bool* result)
{
	token_range_t range = { tok, _find_eol(tok)->next };

	uint32_t val;
	if (!pre_proc_eval_expr(_context, range, &val))
	{
		diag_err(range.start, ERR_SYNTAX, "cannot parse constant expression %s",
			tok_kind_spelling(tok_identifier));
		return NULL;
	}
	*result = val != 0;
	return range.end;
}

/*
Process a conditional pp expression
tok points at one of: tok_pp_if, tok_pp_ifdef, tok_pp_ifndef
returns the next token to be processed
*/
static token_t* _process_condition(token_t* tok)
{
	bool inc_group = false;
	if (tok->kind == tok_pp_ifdef)
	{
		tok = tok->next;
		if (tok->kind != tok_identifier)
		{
			diag_err(tok, ERR_SYNTAX, "expected %s after #ifdef",
				tok_kind_spelling(tok_identifier));
			return NULL;
		}
		inc_group = _find_def(tok) != NULL;
		tok = _find_eol(tok)->next;
	}
	else if (tok->kind == tok_pp_ifndef)
	{
		tok = tok->next;
		if (tok->kind != tok_identifier)
		{
			diag_err(tok, ERR_SYNTAX, "expected %s after #ifndef",
				tok_kind_spelling(tok_identifier));
			return NULL;
		}
		inc_group = _find_def(tok) == NULL;
		tok = _find_eol(tok)->next;
	}
	else if (tok->kind == tok_pp_if)
	{
		if (!(tok = _eval_pp_expression(tok->next, &inc_group)))
			return NULL;
	}
	else
	{
		diag_err(tok, ERR_SYNTAX, "unexpected token looking for pp conditional");
		return NULL;
	}

	//have we hit a true case yet?
	bool any_true = inc_group;
	while (tok->kind != tok_pp_endif)
	{
		if (inc_group)
			any_true = true;

		token_t* next = tok->next;
		if (tok->kind == tok_pp_elif)
		{
			inc_group = !any_true;
			if (!(next = _eval_pp_expression(tok->next, &inc_group)))
				return NULL;
		}
		else if (tok->kind == tok_pp_else)
		{
			inc_group = !any_true;						
			if (next->kind == tok_if)
			{
				if (!(next = _eval_pp_expression(next->next, &inc_group)))
					return NULL;
			}
		}
		else if (inc_group)
		{
			next = _process_token(tok);
		}
		
		tok = next;
	}
	return tok->next;
}

static token_t* _process_token(token_t* tok)
{
	token_t* next = tok->next;

	switch (tok->kind)
	{
	case tok_pp_include:
		next = _process_include(tok);
		break;
	case tok_pp_define:
		next = _process_define(tok);
		break;
	case tok_pp_if:
	case tok_pp_ifdef:
	case tok_pp_ifndef:
		next = _process_condition(tok);
		break;
	case tok_pp_undef:
		next = _process_undef(tok);
		break;
	case tok_pp_null:
		//consume
		next = tok->next;
		break;
	case tok_identifier:
	{
		next = tok->next;
		token_range_t range = _expand_identifier(tok);
		_emit_range(&range);
		break;
	}
	default:
		_emit_token(tok);
		break;
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