#include "pp.h"
#include "pp_internal.h"
#include "diag.h"
#include "source.h"
#include "lexer.h"
#include "ast.h"

#include <string.h>

typedef enum
{
	macro_obj,
	macro_fn
}macro_kind;

typedef struct
{
	macro_kind kind;
	token_range_t tokens;
	token_t* fn_params;
	bool expanding;
}macro_t;

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

static macro_t* _find_def(token_t* ident)
{
	return (macro_t*)sht_lookup(_context->defs, ident->data.str);
}

static token_range_t _expand_identifier(token_t* tok)
{
	macro_t* macro = _find_def(tok);
	if (macro && !macro->expanding)
	{
		return macro->tokens;
	}

	token_range_t r = { tok, tok->next };
	return r;
}

static token_t* _process_undef(token_t* tok)
{
	token_t* identifier = tok->next;
	token_t* end = _find_eol(tok->next)->next;

	if (identifier->kind != tok_identifier)
		return _diag_expected(identifier, tok_identifier);

	char* name = identifier->data.str;

	macro_t* macro = (macro_t*)sht_lookup(_context->defs, name);
	if (macro)
	{
		sht_remove(_context->defs, name);
		free(macro);
	}
	return end;
}

static inline bool _leadingspace_or_startline(token_t* tok)
{
	return tok->flags & TF_LEADING_SPACE || tok->flags & TF_START_LINE;
}

static token_t* _process_fn_define(token_t* identifier)
{
	token_t* tok = identifier->next->next;
	token_t* end = _find_eol(identifier)->next;

	macro_t* macro = (macro_t*)malloc(sizeof(macro_t));
	memset(macro, 0, sizeof(macro_t));
	macro->kind = macro_fn;
	
	token_t* last_param = NULL;
	while (tok->kind == tok_identifier)
	{
		token_t* next = tok->next;

		if (macro->fn_params == NULL)
		{
			macro->fn_params = last_param = tok;
			macro->fn_params->prev = macro->fn_params->next = NULL;
		}
		else
		{
			last_param->next = tok;
			tok->prev = last_param;
			last_param = tok;
		}
		tok->next = NULL;

		tok = next;
		if (tok->kind != tok_comma)
			break;
		tok = tok->next;
	}
	if (tok->kind != tok_r_paren)
	{
		_diag_expected(tok, tok_r_paren);
		free(macro);
		return NULL;
	}
	tok = tok->next;

	macro->tokens.start = tok;
	macro->tokens.end = end;

	macro_t* existing = (macro_t*)sht_lookup(_context->defs, identifier->data.str);
	if (existing)
	{
	}

	sht_insert(_context->defs, identifier->data.str, macro);
	return macro->tokens.end;
}

static token_t* _process_define(token_t* tok)
{
	token_t* end = _find_eol(tok->next)->next;
	token_t* identifier = tok->next;

	//must be a token
	if (identifier->kind != tok_identifier)
		return _diag_expected(identifier, tok_identifier);

	if (identifier->next->kind == tok_l_paren && !(identifier->next->flags & TF_LEADING_SPACE))
	{
		//function-like macro...
		return _process_fn_define(identifier);
	}

	char* name = identifier->data.str;

	//There shall be white-space between the identifier and the replacement list in the definition of an object-like macro.
	if (!_leadingspace_or_startline(identifier->next))
	{
		diag_err(tok, ERR_SYNTAX, "expected white space after macro name '%s'", name);
	}

	macro_t* macro = (macro_t*)malloc(sizeof(macro_t));
	memset(macro, 0, sizeof(macro_t));
	macro->kind = macro_obj;
	macro->tokens.start = identifier->next;
	macro->tokens.end = end;

	macro_t* existing = (macro_t*)sht_lookup(_context->defs, name);
	if (existing)
	{
		/*	An identifier currently defined as an object-like macro shall not be redefined by another
			#define preprocessing directive unless the second definition is an object-like macro
			definition and the two replacement lists are identical. */
		if (existing->kind == macro_obj && tok_range_equals(&existing->tokens, &macro->tokens))
		{	
			free(macro);
			return end;
		}
		free(macro);
		diag_err(tok, ERR_SYNTAX, "redefinition of macro '%s'", name);
		return NULL;
	}

	sht_insert(_context->defs, name, macro);

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
		//strcpy(path, (const char*)range.start->data);
		strcpy(path, range.start->data.str);
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
		strncpy(path, tok->data.str, 1024);
		tok = tok->next;

		if (tok->kind == tok_fullstop)
		{
			strncat(path, ".", 1);
			tok = tok->next;
			if (tok->kind == tok_identifier)
			{
				char buff[128];
				strncpy(buff, tok->data.str, 128);
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

	token_range_t toks = lex_source(sr);
	if (!toks.start)
		return false;
	tok = toks.start;

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

static token_t* _find_fn_param_end(token_t* tok)
{
	int paren_count = 0;

	while (!( (tok->kind == tok_comma || tok->kind == tok_r_paren) && paren_count == 0))
	{
		if (!tok)
			return NULL;

		if (tok->kind == tok_l_paren)
			paren_count++;
		if (tok->kind == tok_r_paren)
			paren_count--;

		//Within the sequence of preprocessing tokens making up an invocation of a function-like macro,
		//new-line is considered a normal white-space character
		if (tok->flags & TF_START_LINE)
			tok->flags = TF_LEADING_SPACE;

		tok = tok->next;
	}
	return tok;
}


static void _emit_replacement_list(token_t* replace, token_range_t* list)
{
	token_t* tok = list->start;
	tok->flags = replace->flags; //first token in the replacement list should inherit flags from the token being replaced
	while (tok != list->end)
	{
		tok = _process_token(tok);
	}
}

static token_t* _process_fn_macro_call(token_t* identifier, macro_t* macro)
{
	token_t* tok = identifier->next;
	if (tok->kind != tok_l_paren)
		return _diag_expected(tok, tok_l_paren);
	tok = tok->next;

	hash_table_t* params = sht_create(8);
	token_t* result = NULL;

	//process the parameters and build a table of param name to token range
	token_t* param = macro->fn_params;
	while (1)
	{
		if (!param)
		{
			//error
			break;
		}

		token_t* end = _find_fn_param_end(tok);

		if (!end)
		{

		}

		token_range_t* range = (token_range_t*)malloc(sizeof(token_range_t));
		range->start = tok;
		range->end = end;

		sht_insert(params, param->data.str, range);

		if (end->kind == tok_r_paren)
		{
			result = end->next;
			break;
		}
		param = param->next;
		tok = end->next;
	}	

	//process the replacement list
	token_t* next;
	tok = macro->tokens.start;
	tok->flags = identifier->flags; //first token in the replacement list should inherit flags from the identifier being replaced	
	while (tok != macro->tokens.end)
	{
		if (tok->kind == tok_identifier)
		{
			//check params
			token_range_t* range = (token_range_t*)sht_lookup(params, tok->data.str);

			if (range)
			{
				_emit_replacement_list(tok, range);
				tok = tok->next;
				/*
				token_t* tok2 = range->start;
				tok2->flags = tok->flags; //first token in the replacement list should inherit flags from the identifier being replaced	
				while (tok2 != range->end)
				{
					next = _process_token(tok2);
					tok2 = next;
				}

				tok = tok->next;*/
				continue;
			}
		}

		next = _process_token(tok);
		tok = next;
	}
	return result;
}

static token_t* _process_identifier(token_t* identifier)
{
	token_t* next = identifier->next;

	macro_t* macro = _find_def(identifier);
	if (macro && !macro->expanding)
	{
		macro->expanding = true;

		if (macro->kind == macro_obj)
		{
			_emit_replacement_list(identifier, &macro->tokens);

			/*token_t* tok = macro->tokens.start;
			tok->flags = identifier->flags; //first token in the replacement list should inherit flags from the indetifier being replaced
			while (tok != macro->tokens.end)
			{
				token_t* next = _process_token(tok);
				tok = next;
			}*/
		}
		else
		{
			next = _process_fn_macro_call(identifier, macro);
		}
		macro->expanding = false;
	}
	else
	{
		_emit_token(identifier);
	}
	return next;
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
		next = _process_identifier(tok);
		break;
	default:
		_emit_token(tok);
		break;
	}
	return next;
}

token_range_t pre_proc_file(token_t* toks)
{
	token_t* tok = toks;

	while (tok)
	{
		tok = _process_token(tok);
	}
	token_range_t result = { _context->first , _context->last };
	return result;
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