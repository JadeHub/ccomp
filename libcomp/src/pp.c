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
	dest->next_tok_flags = FLAGS_UNSET;
	dest->next = _context->dest_stack;
	_context->dest_stack = dest;
}

static void _pop_dest()
{
	dest_range_t* dest = _context->dest_stack;
	assert(dest);
	//_context->dest_stack->next->next_tok_flags = _context->dest_stack->next_tok_flags;
	_context->dest_stack = _context->dest_stack->next;
	assert(_context->dest_stack); //result should never be popped
	free(dest);
}

static void _set_next_tok_flags(uint8_t flags)
{
	if(_context->dest_stack->next_tok_flags == FLAGS_UNSET)
		_context->dest_stack->next_tok_flags = flags;
	//else
		//_context->dest_stack->next_tok_flags = FLAGS_UNSET;
}

static token_t* _peek_next()
{
	assert(_context->expansion_stack);

	expansion_context_t* me = _context->expansion_stack;

	while (me)
	{
		if (me->current != me->tokens->end || me->next == NULL)
			return me->current;
		me = me->next;
	}
	assert(false);
	return NULL;

}

static token_t* _pop_next()
{
	assert(_context->expansion_stack);

	expansion_context_t* me = _context->expansion_stack;

	while (me)
	{
		if (me->current != me->tokens->end || me->next == NULL)
			break;

		//we have eaten all the tokens
		expansion_context_t* tmp = me->next;
		free(me);
		_context->expansion_stack = tmp;
		me = tmp;
	}

	token_t* tok = me->current;
	me->current = me->current->next;

	if (me->next)
	{
		//not the root level means we are expanding a macro or param, so duplicate tok
		return tok_duplicate(tok);
	}
	return tok;
}

/****************************************************************************/

static expansion_context_t* _begin_macro_expansion(macro_t* macro, hash_table_t* params)
{
	expansion_context_t* me = (expansion_context_t*)malloc(sizeof(expansion_context_t));
	memset(me, 0, sizeof(expansion_context_t));
	me->macro = macro;
	me->params = params;
	me->tokens = &macro->tokens;
	me->current = me->tokens->start;
	me->next = _context->expansion_stack;
	_context->expansion_stack = me;
	return me;
}

static expansion_context_t* _begin_token_range_expansion(token_range_t* range)
{
	expansion_context_t* me = (expansion_context_t*)malloc(sizeof(expansion_context_t));
	memset(me, 0, sizeof(expansion_context_t));
	me->tokens = range;
	me->current = me->tokens->start;
	me->next = _context->expansion_stack;
	_context->expansion_stack = me;
	return me;
}

static inline bool _is_expansion_at_end(expansion_context_t* me)
{
	return me->current == me->tokens->end;
}

static bool _is_macro_expanding(macro_t* macro)
{
	expansion_context_t* me = _context->expansion_stack;

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
	expansion_context_t* me = _context->expansion_stack;

	while (me)
	{
		assert(_is_expansion_at_end(me));
		expansion_context_t* next = me->next;
		free(me);
		me = next;
	}
	_context->expansion_stack = NULL;
}

static bool _is_expansion_complete(expansion_context_t* expansion)
{
	if (!_context->expansion_stack)
		assert(_context->expansion_stack);

	expansion_context_t* me = _context->expansion_stack;

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

	if (_context->dest_stack->next_tok_flags != FLAGS_UNSET)
	{
		tok->flags = _context->dest_stack->next_tok_flags;
		_context->dest_stack->next_tok_flags = FLAGS_UNSET;
	}

	if(_context->expansion_stack && _context->expansion_stack->macro)
	{
		phs_insert(_context->expansion_stack->macro->hidden_toks, (void*)tok->id);
	}

	tok->prev = tok->next = NULL;
	if (range->start == NULL)
	{
		tok->flags |= TF_START_LINE;
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

static inline void* _diag_expected(token_t* tok, tok_kind kind)
{
	diag_err(tok, ERR_SYNTAX, "syntax error: expected '%s' before '%s'",
		tok_kind_spelling(kind), diag_tok_desc(tok));
	return NULL;
}

static inline bool _expect_kind(token_t* tok, tok_kind kind)
{
	if (tok->kind == kind)
		return true;
	_diag_expected(tok, kind);
	return false;
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

	if (!_expect_kind(identifier, tok_identifier))
		return false;

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

	token_range_t param = { NULL, NULL };

	while (tok->kind == tok_identifier)
	{
		tok->next = tok->prev;
		if (param.start == NULL)
		{
			param.start = param.end = tok;
		}
		else
		{
			tok->prev = param.end;
			param.end->next = tok;
			param.end = tok;
		}

		tok = _pop_next();
		if (tok->kind != tok_comma)
			break;
		tok = _pop_next();
	}
	param.end = _create_end_marker(param.end);
	macro->fn_params = param.start; //fix
	
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
		macro->tokens.start->flags = 0;

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

static token_t* _stringize_range(token_range_t* range)
{
	if (!range || range->start == range->end)
		return NULL;

	str_buff_t* sb = sb_create(512);

	sb_append_ch(sb, '\"');

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
			sb_append_ch(sb, '\\');
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
			sb_append_ch(sb, '\\');
			sb_append_ch(sb, '\"');
			break;
		}
		default:
			//sb_append(sb, tok->data.str);
			tok_spelling_append(tok->loc, tok->len, sb);
			break;
		}

		tok = tok->next;
	}
	sb_append_ch(sb, '\"');

	source_range_t sr = { sb->buff, sb->buff + sb->len };

	token_range_t toks = lex_source(&sr);
	//result->len = sb->len + 1;
	//result->loc = result->data.str = sb_release(sb);
	
	return toks.start;
}

static token_range_t* _expand_param_range(token_range_t* range)
{
	if (tok_range_empty(range))
		return range;

	token_range_t* result = (token_range_t*)malloc(sizeof(token_range_t));
	memset(result, 0, sizeof(token_range_t));

	//save and reset the expansion stack to prevent looking past the token range
	expansion_context_t* prev_me_stack = _context->expansion_stack;
	_context->expansion_stack = NULL;
	//uint8_t nf = _context->next_tok_flags;
	//_context->next_tok_flags = FLAGS_UNSET;

	//setup the unexpanded param tokens for expansion
	expansion_context_t* me = _begin_token_range_expansion(range);

	_push_dest(result);

	while (!_is_expansion_complete(me))
	{
		if (!_process_token(_pop_next()))
			return NULL;
	}

	//restore the expansion stack
	_destroy_expansion_stack();
	_context->expansion_stack = prev_me_stack;
	//_context->next_tok_flags = nf;

	_pop_dest();

	result->end = _create_end_marker(result->end);
	return result;
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
	while (tok->kind != tok_r_paren)
	{
		if (!param)
			return false; //todo error

		token_range_t* range = (token_range_t*)malloc(sizeof(token_range_t));
		memset(range, 0, sizeof(token_range_t));
		range->start = range->end = NULL;

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

			if (range->start == NULL)
			{
				tok->next = tok->prev = NULL;
				range->start = range->end = tok;
			}
			else
			{
				range->end->next = tok;
				tok->prev = range->end;
				range->end = tok;
				tok->next = NULL;
			}

			tok = _pop_next();
		}
		if(range->start == NULL)
		{
			range->start = range->end = _create_end_marker(NULL);
		}
		else
		{
			range->end = _create_end_marker(range->end);
		}
//		tok_print_range(range);
		sht_insert(params, param->data.str, range);
		if (tok->kind == tok_r_paren)
		{
			break;
		}
		param = param->next;
		tok = _pop_next();
	}

	param = macro->fn_params;
	while (param && param->kind != tok_pp_end_marker)
	{
		if (!sht_lookup(params, param->data.str))
		{
			//add empty tok range for missing parameter
			token_range_t* range = (token_range_t*)malloc(sizeof(token_range_t));
			memset(range, 0, sizeof(token_range_t));
			range->start = range->end = _create_end_marker(NULL);

			sht_insert(params, param->data.str, range);
		}
		param = param->next;
	}

	return params;
}

static token_range_t* _lookup_fn_param(const char* name)
{
	expansion_context_t* expansion = _context->expansion_stack;

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

	return !ht_contains(macro->hidden_toks, (void*)identifier->id);

	return true;
}

static bool _process_token_range(token_range_t* range)
{
	expansion_context_t* me = _begin_token_range_expansion(range);

	while (!_is_expansion_complete(me))
	{
		if (!_process_token(_pop_next()))
			return false;
	}
	return true;
}

static bool _process_arg_substitution(token_t* identifier, token_range_t* param)
{
	_set_next_tok_flags(identifier->flags);

	if (tok_range_empty(param))
	{
		_context->dest_stack->next_tok_flags = FLAGS_UNSET;
		return true;
	}

	
	token_range_t* range = _expand_param_range(param);
	//range->start->flags = identifier->flags;

	//_set_next_tok_flags(identifier->flags);

	//if(_context->next_tok_flags == FLAGS_UNSET)
	//	_context->next_tok_flags = identifier->flags;

	return _process_token_range(range);
}

static bool _process_replacement_list(token_t* replaced, macro_t* macro, hash_table_t* params)
{
	expansion_context_t* me = _begin_macro_expansion(macro, params);
	//macro->tokens.start->flags = replaced->flags;
	//if (_context->next_tok_flags == FLAGS_UNSET)
//		_context->next_tok_flags = replaced->flags;

	_set_next_tok_flags(replaced->flags);

	while (!_is_expansion_complete(me))
	{
		if (!_process_token(_pop_next()))
			return false;
	}

	return true;
}

static bool _process_identifier(token_t* identifier)
{
	//is this identifier the parameter of a macro fn being expanded?
	token_range_t* p_range = _lookup_fn_param(identifier->data.str);
	if (p_range)
	{
		return _process_arg_substitution(identifier, p_range);
	}

	macro_t* macro = _find_macro_def(identifier);
	if (macro && _should_expand_macro(identifier, macro))
	{
		if (macro->kind == macro_obj)
		{
			return _process_replacement_list(identifier, macro, NULL);
		}
		//identifier maps to a function like macro but we only invoke it if it looks like a call
		else if (_peek_next()->kind == tok_l_paren)
		{
			hash_table_t* params = _process_fn_call_params(macro);
			if (!params)
				return false;

			return _process_replacement_list(identifier, macro, params);
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

	_set_next_tok_flags(hash->flags);

	//if (_context->next_tok_flags == FLAGS_UNSET)
//		_context->next_tok_flags = hash->flags;

	//tok->flags = hash->flags;

	return _process_token(tok);
}

static bool _is_valid_pasted_tok(token_range_t* range)
{
	return (range &&
		range->start &&
		range->start->kind != tok_invalid &&
		range->start->kind != tok_eof &&
		range->start->next &&
		range->start->next->kind == tok_eof);
}

static token_t* _paste_tokens(token_t* lhs, token_t* rhs)
{
	if (!lhs)
		return rhs;
	if (!rhs)
		return lhs;

	str_buff_t* sb = sb_create(64);

	tok_spelling_append(lhs->loc, lhs->len, sb);
	tok_spelling_append(rhs->loc, rhs->len, sb);

	source_range_t sr = { sb->buff, sb->buff + sb->len };

	token_range_t toks = lex_source(&sr);
	if (_is_valid_pasted_tok(&toks))
	{
		toks.start->flags = lhs->flags;
		return toks.start;
	}

	return _is_valid_pasted_tok(&toks) ? toks.start : NULL;
}

static bool _process_hashhash(token_t* first)
{
	if (!_expect_kind(_pop_next(), tok_hashhash))
		return false;

	uint8_t flags = first->flags;

	token_t* second = _pop_next();

	if (first->kind == tok_identifier)
	{
		token_range_t* p_range = _lookup_fn_param(first->data.str);
		if (p_range)
		{
			if (tok_range_empty(p_range))
			{
				first = NULL; //the 'placemarker' token
			}
			else
			{
				p_range->start->flags = first->flags;
				//first arg is a macro param, output all but the final token
				p_range->end = p_range->end->prev;
				if (!tok_range_empty(p_range))
					if (!_process_token_range(p_range))
						return false;
				first = p_range->end;
			}
		}
	}

	if (second->kind == tok_identifier)
	{
		token_range_t* p_range = _lookup_fn_param(second->data.str);
		if (p_range && !tok_range_empty(p_range))
		{
			if (tok_range_empty(p_range))
			{
				second = NULL; //the 'placemarker' token
			}
			else
			{
				//second arg is a macro param, use the first token in the pasting

				second = p_range->start;
				p_range->start = p_range->start->next;

				token_t* result = _paste_tokens(first, second);
				//result->flags = flags;
				if (!_process_token(result))
					return false;

				if (!_process_token_range(p_range))
					return false;
				first = p_range->end;
				return true;
			}
		}
	}

	token_t* result = _paste_tokens(first, second);

	if (result)
	{
		return _process_token(result);
	}

	//todo warning

	return _process_token(first) && _process_token(second);
}

static bool _process_token(token_t* tok)
{
	//Check for ## operator if we are processing a macro's replacement list
	if (_context->expansion_stack->macro && _peek_next()->kind == tok_hashhash)
	{
		return _process_hashhash(tok);
	}

	switch (tok->kind)
	{
	case tok_hash:
		//consume
		return _process_hash_op(tok);
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

	_begin_token_range_expansion(range);

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

	_push_dest(&_context->result);

	//_context->dest_stack = (dest_range_t*)malloc(sizeof(dest_range_t));
	//memset(_context->dest_stack, 0, sizeof(dest_range_t));
	//_context->dest_stack->range = &_context->result;
	//_context->next_tok_flags = FLAGS_UNSET;
}

void pre_proc_deinit()
{
	ht_destroy(_context->defs);
	free(_context);
	_context = NULL;
}