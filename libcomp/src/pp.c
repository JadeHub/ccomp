#include "pp.h"
#include "pp_internal.h"
#include "diag.h"
#include "source.h"
#include "lexer.h"
#include "ast.h"

#include <libj/include/str_buff.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>

static bool _process_token(token_t* tok);
static pp_context_t* _context = NULL;

typedef void (*emit_fn)(token_t*, token_range_t* range);

/***************************************************/

static void _push_dest(token_range_t* range)
{
	dest_range_t* dest = (dest_range_t*)malloc(sizeof(dest_range_t));
	memset(dest, 0, sizeof(dest_range_t));
	dest->range = range;
	dest->next = _context->dest_stack;
	_context->dest_stack = dest;
}

static void _pop_dest()
{
	dest_range_t* dest = _context->dest_stack;
	assert(dest);
	_context->dest_stack = _context->dest_stack->next;
	assert(_context->dest_stack); //result should never be popped
	free(dest);
}

static token_t* _peek_next()
{
	macro_expansion_t* me = _context->expansion_stack;

	while (me)
	{
		//we can't 'look through' parameters to the following tokens
		if (me->current != me->tokens->end || me->macro == NULL)
			return me->current;
		me = me->next;
	}
	return _context->current;
}

static token_t* _pop_next_from(macro_expansion_t* me)
{
	while (me)
	{
		if (me->current != me->tokens->end)
			break;

		//we have eaten all the tokens
		macro_expansion_t* tmp = me->next;
		free(me);
		_context->expansion_stack = tmp;
		me = tmp;
	}

	if (me)
	{
		token_t* tok = me->current;
		me->current = me->current->next;
		return tok_duplicate(tok);
	}

	//take from input
	token_t* tok = _context->current;
	if (_context->current != _context->input.end)
		_context->current = _context->current->next;

	return tok;
}

static token_t* _pop_next()
{
	return _pop_next_from(_context->expansion_stack);
}

/****************************************************************************/

static void _begin_macro_expansion(macro_t* macro)
{
	macro_expansion_t* me = (macro_expansion_t*)malloc(sizeof(macro_expansion_t));
	memset(me, 0, sizeof(macro_expansion_t));
	me->macro = macro;
	me->params = NULL;
	me->tokens = &macro->tokens;
	me->current = me->macro->tokens.start;
	me->next = _context->expansion_stack;
	_context->expansion_stack = me;
}

static macro_expansion_t* _begin_fn_param_expansion(token_range_t* range)
{
	macro_expansion_t* me = (macro_expansion_t*)malloc(sizeof(macro_expansion_t));
	memset(me, 0, sizeof(macro_expansion_t));
	me->tokens = range;
	me->current = range->start;
	me->next = _context->expansion_stack;
	_context->expansion_stack = me;
	return me;
}

static inline bool _is_expansion_at_end(macro_expansion_t* me)
{
	return me->current == me->tokens->end;
}

static bool _is_macro_expanding(macro_t* macro)
{
	macro_expansion_t* me = _context->expansion_stack;

	while (me)
	{
		if (me->macro == macro)
			return true;
		me = me->next;
	}
	return false;
}

static void _destroy_expansion_stack()
{
	macro_expansion_t* me = _context->expansion_stack;

	while (me)
	{
		assert(_is_expansion_at_end(me));
		macro_expansion_t* next = me->next;
		free(me);
		me = next;
	}
	_context->expansion_stack = NULL;
}

static bool _is_expansion_complete(macro_expansion_t* expansion)
{
	if (!_context->expansion_stack)
		assert(_context->expansion_stack);

	macro_expansion_t* me = _context->expansion_stack;

	while (me)
	{
		if (!_is_expansion_at_end(me)) //either expansion, or something before it is not complete
			return false;

		if (me == expansion)
			return _is_expansion_at_end(me);

		me = me->next;
	}
	//no longer in the stack, must be complete
	return true;
}

static inline void _emit_token(token_t* tok)
{
	token_range_t* range = _context->dest_stack->range;

	if(_context->expansion_stack && _context->expansion_stack->macro)
	{
		phs_insert(_context->expansion_stack->macro->hidden_toks, (void*)tok->id);
	}

	tok->prev = tok->next = NULL;
	if (range->start == NULL)
	{
		range->start = range->end = tok;
	}
	else
	{
		range->end->next = tok;
		tok->prev = range->end;
		range->end = tok;
	}
}

static token_t* _create_end_marker(token_t* param_end)
{
	token_t* end = tok_create();
	end->kind = tok_pp_end_marker;
	if(param_end)
		param_end->next = end;
	end->prev = param_end;

	return end;
}

static inline bool _expect_kind(token_t* tok, tok_kind kind)
{
	if (tok->kind == kind)
		return true;
	diag_err(tok, ERR_SYNTAX, "syntax error: expected '%s' before '%s'",
		tok_kind_spelling(kind), diag_tok_desc(tok));
	return false;
}

static inline void* _diag_expected(token_t* tok, tok_kind kind)
{
	diag_err(tok, ERR_SYNTAX, "syntax error: expected '%s' before '%s'",
		tok_kind_spelling(kind), diag_tok_desc(tok));
	return NULL;
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

static macro_t* _find_macro_def(token_t* ident)
{
	return (macro_t*)sht_lookup(_context->defs, ident->data.str);
}

static inline bool _leadingspace_or_startline(token_t* tok)
{
	return tok->flags & TF_LEADING_SPACE || tok->flags & TF_START_LINE;
}

static bool _process_undef(token_t* tok)
{
	token_t* identifier = _pop_next();

	if (identifier->kind != tok_identifier)
		return _diag_expected(identifier, tok_identifier);

	char* name = identifier->data.str;

	macro_t* macro = (macro_t*)sht_lookup(_context->defs, name);
	if (macro)
	{
		sht_remove(_context->defs, name);
		free(macro);
	}
	return true;
}

/*
When called the opening l_paren should be the first token waiting in the input stream
*/
static bool _process_fn_params(macro_t* macro)
{
	token_t* tok = _pop_next();
	assert(tok->kind == tok_l_paren);

	//Process parameters
	tok = _pop_next();
	token_t* last_param = NULL;
	while (tok->kind == tok_identifier)
	{
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

		tok = _pop_next();
		if (tok->kind != tok_comma)
			break;
		tok = _pop_next();
	}
	
	if (tok->kind != tok_r_paren)
	{
		_diag_expected(tok, tok_r_paren);
		free(macro);
		return false;
	}
	return true;
}

static bool _process_define(token_t* def)
{
	token_t* identifier = _pop_next();

	//must be a token
	if (!_expect_kind(identifier, tok_identifier))
		return false;

	macro_t* macro = (macro_t*)malloc(sizeof(macro_t));
	memset(macro, 0, sizeof(macro_t));
	macro->kind = macro_obj;
	macro->hidden_toks = phs_create(64);
	macro->name = identifier->data.str;

	token_t* tok = _peek_next();

	if (tok->kind == tok_l_paren && !(tok->flags & TF_LEADING_SPACE))
	{
		macro->kind = macro_fn;
		if (!_process_fn_params(macro))
		{
			free(macro);
			return false;
		}
		tok = _peek_next();
	}
	else
	{
		macro->kind = macro_obj;
		//There shall be white-space between the identifier and the replacement list in the definition of an object-like macro.
		if (!_leadingspace_or_startline(tok))
		{
			diag_err(tok, ERR_SYNTAX, "expected white space after macro name '%s'", macro->name);
			free(macro);
			return false;
		}
	}

	if (tok->flags & TF_START_LINE)
	{
		//empty replacement list
		macro->tokens.start = macro->tokens.end = _create_end_marker(NULL);
	}
	else
	{
		macro->tokens.start = macro->tokens.end = tok;
		while (!(tok->flags & TF_START_LINE))
		{
			macro->tokens.end = _pop_next();
			tok = _peek_next();
		}
		macro->tokens.end = _create_end_marker(macro->tokens.end);
	}

	if (macro->tokens.start != macro->tokens.end)
	{
		/*
			A ## preprocessing token shall not occur at the beginning or at the end of a replacement
			list for either form of macro definition
		*/
		if (macro->tokens.start->kind == tok_hashhash)
		{
			diag_err(macro->tokens.start, ERR_SYNTAX,
				"macro replacement list may not start with '##'");
			return false;
		}
		if(macro->tokens.end->prev->kind == tok_hashhash)
		{
			diag_err(macro->tokens.end->prev, ERR_SYNTAX,
				"macro replacement list may not end with '##'");
			return false;
		}
	}

	macro_t* existing = (macro_t*)sht_lookup(_context->defs, macro->name);
	if (existing)
	{
		/*
			An identifier currently defined as an object-like macro shall not be redefined by another
			#define preprocessing directive unless the second definition is an object-like macro
			definition and the two replacement lists are identical.
		*/
		if (existing->kind == macro_obj && tok_range_equals(&existing->tokens, &macro->tokens))
		{	
			free(macro);
			return true;
		}
		diag_err(tok, ERR_SYNTAX, "redefinition of macro '%s'", macro->name);
		free(macro);
		return false;
	}

	sht_insert(_context->defs, macro->name, macro);
	return true;
}

static bool _process_include(token_t* tok)
{
/*	tok = _pop_next();
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
	
	return end;*/
	return true;
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
*/
static bool _process_condition(token_t* tok)
{
	/*bool inc_group = false;
	if (tok->kind == tok_pp_ifdef)
	{
		tok = tok->next;
		if (tok->kind != tok_identifier)
		{
			diag_err(tok, ERR_SYNTAX, "expected %s after #ifdef",
				tok_kind_spelling(tok_identifier));
			return NULL;
		}
		inc_group = _find_macro_def(tok) != NULL;
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
		inc_group = _find_macro_def(tok) == NULL;
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
	return tok->next;*/
	return true;
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

static token_t* _stringize_range(token_range_t* range)
{
	if (!range || range->start == range->end)
		return NULL;

	token_t* result = tok_create();
	result->kind = tok_string_literal;

	str_buff_t* sb = sb_create(512);

	token_t* tok = range->start;
	while (tok != range->end)
	{
		/*
			Each occurrence of white space between the argument’s preprocessing tokens
			becomes a single space character in the character string literal. White space before the
			first preprocessing token and after the last preprocessing token composing the argument
			is deleted.
		*/
		if (tok != range->start && _leadingspace_or_startline(tok))
			sb_append_ch(sb, ' ');

		switch (tok->kind)
		{
		case tok_string_literal:
		{
			sb_append_ch(sb, '\"');
			char* c = tok->data.str;
			while (*c)
			{
				// a \ character is inserted before each " and \ character of a character constant or string literal
				if (*c == '\\' || *c == '\"')
					sb_append_ch(sb, '\\');
				sb_append_ch(sb, *c);
				c++;
			}
			sb_append_ch(sb, '\"');
			break;
		}
		default:
			//sb_append(sb, tok->data.str);
			tok_spelling_extract(tok->loc, tok->len, sb);
			break;
		}

		tok = tok->next;
	}

	result->len = sb->len + 1;
	result->loc = result->data.str = sb_release(sb);
	return result;
}

static token_range_t* _expand_range(token_range_t* range)
{
	token_range_t* result = (token_range_t*)malloc(sizeof(token_range_t));
	memset(result, 0, sizeof(token_range_t));

	//save and reset the expansion stack
	macro_expansion_t* prev_me_stack = _context->expansion_stack;
	_context->expansion_stack = NULL;

	//setup the unexpanded param tokens for expansion
	macro_expansion_t* me = _begin_fn_param_expansion(range);

	_push_dest(result);

	token_t* tok;
	while (!_is_expansion_complete(me))
	{
		tok = _pop_next();
		_process_token(tok);
	}

	//restore the expansion stack
	_destroy_expansion_stack();
	_context->expansion_stack = prev_me_stack;

	_pop_dest();

	result->end = _create_end_marker(result->end);
	return result;
}

static void _process_arg_substitution(token_t* identifier, token_range_t* param)
{
	macro_t* macro = _context->expansion_stack->macro;

	token_range_t* range = _expand_range(param);

	range->start->flags = identifier->flags;
	macro_expansion_t* me = _begin_fn_param_expansion(range);
	me->macro = macro;
}

/*
Returns a sht of param name -> token_range_t*
*/
static hash_table_t* _process_fn_call_params(macro_t* macro)
{
	token_t* tok = _pop_next();
	if (!_expect_kind(tok, tok_l_paren))
		return false;
	
	tok = _pop_next();

	hash_table_t* params = sht_create(8);
	token_t* result = NULL;

	//process the parameters and build a table of param name to token range
	token_t* param = macro->fn_params;
	//token_t* last = param;
	while (tok->kind != tok_r_paren)
	{
		if (!param)
			return false; //todo error

		token_range_t* range = (token_range_t*)malloc(sizeof(token_range_t));
		memset(range, 0, sizeof(token_range_t));
		range->start = range->end = tok;

		int paren_count = 0;
		while (!((tok->kind == tok_comma || tok->kind == tok_r_paren) && paren_count == 0))
		{
			if (tok->kind == tok_l_paren)
				paren_count++;
			if (tok->kind == tok_r_paren)
				paren_count--;

			//Within the sequence of preprocessing tokens making up an invocation of a function-like macro,
			//new-line is considered a normal white-space character
			if (tok->flags & TF_START_LINE)
				tok->flags = TF_LEADING_SPACE;

			range->end->next = tok;
			tok->prev = range->end;
			range->end = tok;
			tok->next = NULL;

			tok = _pop_next();
		}
		range->end = _create_end_marker(range->end);
//		tok_print_range(range);
		sht_insert(params, param->data.str, range);
		if (tok->kind == tok_r_paren)
		{
			break;
		}
		param = param->next;
		tok = _pop_next();
	}

	return params;
}

static token_range_t* _lookup_fn_param(const char* name)
{
	macro_expansion_t* expansion = _context->expansion_stack;

	while (expansion)
	{
		if (expansion->params)
		{
			token_range_t* range = (token_range_t*)sht_lookup(expansion->params, name);
			return range;
		}
		expansion = expansion->next;
	}
	return NULL;
}

static bool _should_expand_macro(token_t* identifier, macro_t* macro)
{
	if (_is_macro_expanding(macro))
		return false;

	if (_context->expansion_stack)
	{
		return !ht_contains(macro->hidden_toks, (void*)identifier->id);
	}

	return true;
}

static bool _process_identifier(token_t* identifier)
{
	//is this identifier the parameter of a macro fn being expanded?
	token_range_t* p_range = _lookup_fn_param(identifier->data.str);
	if (p_range)
	{
		_process_arg_substitution(identifier, p_range);
		return true;
	}

	macro_t* macro = _find_macro_def(identifier);
	if (macro && _should_expand_macro(identifier, macro))
	{
		if (macro->kind == macro_obj)
		{
			_begin_macro_expansion(macro);
			macro->tokens.start->flags = identifier->flags;
			return true;
		}
		//identifier maps to a function like macro but we only invoke it if it looks like a call
		else if (_peek_next()->kind == tok_l_paren)
		{
			// macro->kind == macro_fn
			hash_table_t* params = _process_fn_call_params(macro);
			if (!params)
				return false;

			_begin_macro_expansion(macro);
			_context->expansion_stack->params = params;
			macro->tokens.start->flags = identifier->flags;
			return true;
		}
	}
	_emit_token(identifier);
	return true;
}

static bool _process_hash_op(token_t* hash)
{
	token_t* identifier = _pop_next();

	if (!_expect_kind(identifier, tok_identifier))
		return false;

	token_range_t* p_range = _lookup_fn_param(identifier->data.str);
	if (!p_range)
		return false; //todo error

	token_t* tok = _stringize_range(p_range);
	tok->flags = hash->flags;

	return _process_token(tok);
}

static bool _process_token(token_t* tok)
{
	switch (tok->kind)
	{
	case tok_hash:
		//consume
		return _process_hash_op(tok);
		break;
	case tok_hashhash:
		break;
	case tok_pp_include:
		if (!_process_include(tok))
			return false;
		break;
	case tok_pp_define:
		if (!_process_define(tok))
			return false;
		break;
	case tok_pp_if:
	case tok_pp_ifdef:
	case tok_pp_ifndef:
		if (!_process_condition(tok))
			return false;
		break;
	case tok_pp_undef:
		if (!_process_undef(tok))
			return false;
		break;
	case tok_pp_null:
		//consume
		break;
	case tok_identifier:
		if (!_process_identifier(tok))
			return false;
		break;
	default:
		_emit_token(tok);
		break;
	}
	return true;
}

token_range_t pre_proc_file(token_range_t* range)
{
	_context->input = *range;
	_context->current = range->start;

	token_t* tok = _pop_next();
	while (tok->kind != tok_eof)
	{
		_process_token(tok);
		tok = _pop_next();
	}
	_emit_token(tok);
	return _context->result;
}

void pre_proc_init()
{
	_context = (pp_context_t*)malloc(sizeof(pp_context_t));
	memset(_context, 0, sizeof(pp_context_t));
	_context->defs = sht_create(128);
	_context->dest_stack = (dest_range_t*)malloc(sizeof(dest_range_t));
	memset(_context->dest_stack, 0, sizeof(dest_range_t));
	_context->dest_stack->range = &_context->result;
}

void pre_proc_deinit()
{
	ht_destroy(_context->defs);
	free(_context);
	_context = NULL;
}