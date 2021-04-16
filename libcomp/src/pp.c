#include "pp.h"
#include "pp_internal.h"
#include "diag.h"
#include "source.h"
#include "lexer.h"
#include "ast.h"

#include <libj/include/platform.h>

#include <libj/include/str_buff.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>

static bool _process_token(token_t* tok);
static pp_context_t* _context = NULL;

static token_t* _lex_single_tok(str_buff_t* sb)
{
	source_range_t sr = { sb->buff, sb->buff + sb->len };

	token_range_t range = lex_source(&sr);

	if (range.start && range.start->next == range.end)
	{
		tok_destory(range.end);
		range.start->next = NULL;
		return range.start;
	}
	sb_destroy(sb);
	tok_range_destroy(&range);
	return NULL;
}

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
	_context->dest_stack = _context->dest_stack->next;
	assert(_context->dest_stack); //result should never be popped
	free(dest);
}

static void _set_next_tok_flags(uint8_t flags)
{
	if(_context->dest_stack->next_tok_flags == FLAGS_UNSET)
		_context->dest_stack->next_tok_flags = flags;
}

static token_t* _peek_next()
{
	assert(_context->input_stack);

	input_range_t* ir = _context->input_stack;
	while (ir)
	{
		if (ir->current != ir->tokens->end || ir->next == NULL)
			return ir->current;
		ir = ir->next;
	}
	assert(false);
	return NULL;
}

static token_t* _pop_next()
{
	assert(_context->input_stack);
	
	input_range_t* ir = _context->input_stack;
	while (ir)
	{
		if (ir->current != ir->tokens->end || ir->next == NULL)
			break;

		if (ir->macro_expansion)
			ir->macro_expansion->complete = true;

		_context->input_stack = ir->next;
		free(ir->tokens);
		free(ir);
		ir = _context->input_stack;
	}
	token_t* tok = ir->current;
	ir->current = ir->current->next;

	if (tok->flags & TF_START_LINE)
	{
		ir->line_num = src_file_position(tok->loc).line;
	}

	if (ir->next)
	{
		//not the root level means we are expanding a macro or param, so duplicate tok
		return tok_duplicate(tok);
	}

	return tok;
}

static token_t* _create_end_marker(token_t* param_end)
{
	token_t* end = tok_create();
	end->kind = tok_pp_end_marker;
	if (param_end)
		param_end->next = end;
	end->prev = param_end;

	return end;
}

static token_t* _pop_to_eol(token_t* tok)
{
	while ((_peek_next()->flags & TF_START_LINE) != TF_START_LINE)
		tok = _pop_next();

	return tok;
}

static token_range_t _extract_till_eol(token_t* start)
{
	token_range_t range = { start, start };
	
	while ((_peek_next()->flags & TF_START_LINE) != TF_START_LINE)
	{
		token_t* tok = _pop_next();
		tok_range_add(&range, tok);
	}
	range.end = _create_end_marker(range.end);
	return range;
}

static input_range_t* _begin_token_range_expansion(token_range_t* range)
{
	input_range_t* ir = (input_range_t*)malloc(sizeof(input_range_t));
	memset(ir, 0, sizeof(input_range_t));
	ir->tokens = tok_range_dup(range);
	ir->current = ir->tokens->start;
	ir->next = _context->input_stack;

	ir->path = src_file_path(range->start->loc);
	ir->line_num = src_file_position(range->start->loc).line;

	_context->input_stack = ir;
	return ir;
}

static input_range_t* _begin_macro_expansion(macro_t* macro, hash_table_t* params)
{
	expansion_context_t* me = (expansion_context_t*)malloc(sizeof(expansion_context_t));
	memset(me, 0, sizeof(expansion_context_t));
	me->macro = macro;
	me->params = params;
	input_range_t* input = _begin_token_range_expansion(&macro->tokens);
	input->macro_expansion = me;
	me->next = _context->expansion_stack;
	_context->expansion_stack = me;
	return input;
}

static void _pop_macro_expansion(macro_t* macro)
{
	expansion_context_t* me = _context->expansion_stack;
	assert(me && me->macro == macro);
	_context->expansion_stack = me->next;
	free(me);
}

static inline bool _is_expansion_at_end(input_range_t* ir)
{
	return ir->current == ir->tokens->end;
}

static bool _is_expansion_complete(input_range_t* expansion)
{
	input_range_t* ir = _context->input_stack;

	while (ir)
	{
		if (!_is_expansion_at_end(ir)) //either expansion, or something before it is not complete
			return false;

		if (ir == expansion)
			return _is_expansion_at_end(ir);

		ir = ir->next;
	}
	//no longer in the stack, must be complete
	return true;
}

static bool _is_macro_expanding(macro_t* macro)
{
	expansion_context_t* me = _context->expansion_stack;

	while (me)
	{
		if (me->macro == macro && !me->complete)
			return true;
		me = me->next;
	}
	return false;
}

static void _destroy_input_stack()
{
	input_range_t* ir = _context->input_stack;

	while (ir)
	{
		assert(_is_expansion_at_end(ir));
		input_range_t* next = ir->next;
		free(ir);
		ir = next;
	}
	_context->input_stack = NULL;
}

/*
	We need to supress expansion of identifier tokens when the output sequence contains either:
	define (ID)
	define ID

	dss_normal = Ignore state
	dss_in_cond = In #if or #elif line
	dss_saw_defined = Last tok was tok_identifier 'defined'
	dss_saw_r_paren = dss_saw_defined was followed by tok_l_paren
*/
typedef enum
{
	dss_normal,
	dss_in_cond,		//from tok_pp_if or tok_pp_elif to eol
	dss_saw_defined,	//last tok was tok_identifier with 'defined'
	dss_saw_r_paren		//last tok was tok_r_paren, immediatly after the 'defined'
}define_supress_state;

static void _update_defined_id_supression_state(token_t* tok)
{
	if (_context->define_id_supression_state == dss_normal)
	{
		if (tok->kind == tok_pp_if || tok->kind == tok_pp_elif)
		{
			_context->define_id_supression_state = dss_in_cond;
			return;
		}
		return;
	}

	if (tok->flags & TF_START_LINE)
	{
		//reset
		_context->define_id_supression_state = dss_normal;
		return;
	}

	if (_context->define_id_supression_state == dss_in_cond)
	{
		if (tok->kind == tok_identifier && strcmp(tok->data.str, "defined") == 0)
			_context->define_id_supression_state = dss_saw_defined;
		return;
	}
	
	if (_context->define_id_supression_state == dss_saw_defined)
	{
		if (tok->kind == tok_l_paren)
			_context->define_id_supression_state = dss_saw_r_paren;
		else
			_context->define_id_supression_state = dss_in_cond;
		return;
	}

	if(_context->define_id_supression_state == dss_saw_r_paren)
		_context->define_id_supression_state = dss_in_cond;
}

static inline bool _supress_identifier_expansion(token_t* tok)
{
	if ((_context->define_id_supression_state == dss_saw_defined ||
		_context->define_id_supression_state == dss_saw_r_paren) &&
		tok->kind == tok_identifier)
	{
		_context->define_id_supression_state = dss_in_cond;

		return true;
	}
	return false;
}

static inline void _emit_token(token_t* tok)
{
	_update_defined_id_supression_state(tok);

	token_range_t* range = _context->dest_stack->range;

	if (_context->dest_stack->next_tok_flags != FLAGS_UNSET)
	{
		tok->flags = _context->dest_stack->next_tok_flags;
		_context->dest_stack->next_tok_flags = FLAGS_UNSET;
	}

	if(_context->expansion_stack && _context->expansion_stack->macro)
	{
		size_t id = tok->id;
		phs_insert(_context->expansion_stack->macro->hidden_toks, (void*)id);
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

static bool _process_token_range(token_range_t* range)
{
	input_range_t* ir = _begin_token_range_expansion(range);

	while (!_is_expansion_complete(ir))
	{
		if (!_process_token(_pop_next()))
			return false;
	}
	return true;
}

static macro_t* _find_macro_def(token_t* ident)
{
	return (macro_t*)sht_lookup(_context->defs, ident->data.str);
}

static inline bool _leadingspace_or_startline(token_t* tok)
{
	return tok->flags & TF_LEADING_SPACE || tok->flags & TF_START_LINE;
}

static token_t* _is_standard_macro(token_t* tok)
{
	if (tok->kind != tok_identifier)
		return NULL;

	if (strcmp(tok->data.str, "__FILE__") == 0)
	{
		str_buff_t* sb = sb_create(128);
		sb_append_ch(sb, '\"');
		sb_append(sb, _context->input_stack->path);
		sb_append_ch(sb, '\"');

		return _lex_single_tok(sb);
	}
	else if (strcmp(tok->data.str, "__LINE__") == 0)
	{
		str_buff_t* sb = sb_create(128);
		sb_append_int(sb, _context->input_stack->line_num, 10);
		return _lex_single_tok(sb);
	}
	else if (strcmp(tok->data.str, "__DATE__") == 0)
	{

	}
	else if (strcmp(tok->data.str, "__TIME__") == 0)
	{

	}
	return NULL;
}

static bool _process_undef(token_t* tok)
{
	tok;
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
	macro->define = def;
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
			diag_err(def, ERR_SYNTAX, "expected white space after macro name '%s'", macro->name);
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
		macro->tokens = _extract_till_eol(_pop_next());
	}

	if (!tok_range_empty(&macro->tokens))
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
		diag_err(def, ERR_SYNTAX, "redefinition of macro '%s'. Previously defined at: %s:%s", macro->name,
			src_file_path(existing->define->loc),
			src_file_pos_str(src_file_position(existing->define->loc)));
		free(macro);
		return false;
	}

	sht_insert(_context->defs, macro->name, macro);
	return true;
}

static bool _process_pragma(token_t* tok)
{
	tok = _pop_next();

	if (tok->kind == tok_identifier && strcmp(tok->data.str, "once") == 0)
	{
		const char* path = src_file_path(tok->loc);
		assert(path);
		sht_insert(_context->praga_once_paths, path, (void*)1);
		return true;
	}
	//todo warn
	return false;
}

/*
The path in a #include <....> is lexed as a series of tokens, not as a string literal. We need to form a path from these tokens.
eg
#include <proj/include/header.h>
is
tok_lesser, tok_identifier (proj), tok_slash, tok_identifier (include)..., tok_fullstop, tok_identifier (h), tok_greater

note: '\' will be included in string literals
we are not very strict about the path format here, if it looks resonable we return it and let the file loading code report any error
*/
static str_buff_t* _make_system_inc_path(token_range_t* range)
{
	str_buff_t* path_buff = sb_create(128);

	token_t* tok = range->start;

	assert(tok->kind == tok_lesser);
	tok = tok->next;

	bool saw_fullstop = false;
	while (tok != range->end)
	{
		if(tok->kind == tok_identifier)
		{
			const char* lit = tok->data.str;
			while (*lit)
			{
				if(*lit == '\\')
					sb_append_ch(path_buff, '/');
				else
					sb_append_ch(path_buff, *lit);
				lit++;
			}
		}
		else if (tok->kind == tok_slash)
		{
			sb_append_ch(path_buff, '/');
		}
		else if (tok->kind == tok_fullstop)
		{
			if (saw_fullstop && tok->kind != tok_identifier)
			{
				diag_err(range->start, ERR_SYNTAX, "syntax error: invalid path in #include");
				sb_destroy(path_buff);
				return NULL;
			}
			sb_append_ch(path_buff, '.');
			saw_fullstop = true;
		}
		else if (tok->kind == tok_greater)
		{
			break;
		}
		tok = tok->next;
	}

	if (tok->kind != tok_greater || path_buff->len == 0)
	{
		diag_err(range->start, ERR_SYNTAX, "syntax error: invalid path in #include");
		sb_destroy(path_buff);
		return NULL;
	}

	return path_buff;
}

static bool _process_include(token_t* tok)
{
	tok = _pop_next();
	token_range_t* range = tok_range_create(tok, tok);
	*range = _extract_till_eol(tok);

	if (tok->kind == tok_identifier)
	{
		//#define INC <blah.h>
		//#include INC
		token_range_t* expanded = tok_range_create(NULL, NULL);

		_context->define_id_supression_state = dss_in_cond;

		_push_dest(expanded);
		if (!_process_token_range(range))
			return false;
		_pop_dest();
		expanded->end = _create_end_marker(expanded->end);

		range = expanded;
	}

	str_buff_t* path_buff = sb_create(128);
	include_kind inc_kind = include_local;
	if (range->start->kind == tok_string_literal)
	{
		//#include "blah.h"
		inc_kind = include_local;

		sb_append(path_buff, range->start->data.str);
		if(range->start->next != range->end)
		{
			diag_err(tok, ERR_SYNTAX,
				"expected newline after #include directive");
			sb_destroy(path_buff);
			return false;
		}
	}
	else if (range->start->kind == tok_lesser)
	{
		//#include <blah.h>
		inc_kind = include_system;
		path_buff = _make_system_inc_path(range);
	}
	
	if(path_buff->len == 0)
	{
		diag_err(tok, ERR_SYNTAX, "expected '<path>' or '\"path\"' after #include directive");
		sb_destroy(path_buff);
		return false;
	}

	const char* cur_path = path_dirname(_context->input_stack->path);
	source_range_t* sr = src_load_header(cur_path, sb_str(path_buff), inc_kind);
	free(cur_path);
	if (!src_is_valid_range(sr))
	{
		diag_err(tok, ERR_UNKNOWN_SRC_FILE, "unknown file '%s'", sb_str(path_buff));
		sb_destroy(path_buff);
		return false;
	}

	sb_destroy(path_buff);

	//check if previously #pragma once'd
	const char* inc_path = src_file_path(sr->ptr);
	assert(inc_path);
	if (sht_lookup(_context->praga_once_paths, inc_path))
		return true;

	//lex the file
	token_range_t toks = lex_source(sr);
	if (!toks.start)
		return false;

	return _process_token_range(&toks);
}

/*
Evaluate the condition in a #if or #elif pre proc expression
tok points to the start of the expression
returns the token after the expression
*/
static bool _eval_pp_expression(token_t* start, bool* result)
{
	token_range_t expanded = { NULL, NULL };
	token_range_t range = _extract_till_eol(start);

	_context->define_id_supression_state = dss_in_cond;

	_push_dest(&expanded);
	if (!_process_token_range(&range))
		return false;
	_pop_dest();
	expanded.end = _create_end_marker(expanded.end);

	uint32_t val;
	if (!pre_proc_eval_expr(_context, expanded, &val))
	{
		diag_err(range.start, ERR_SYNTAX, "cannot parse constant expression %s",
			tok_kind_spelling(tok_identifier));
		return false;
	}
	*result = val != 0;
	return true;
}

/*
Process a conditional pp expression
tok points at one of: tok_pp_if, tok_pp_ifdef, tok_pp_ifndef
*/
static bool _process_condition(token_t* tok)
{
	bool inc_group = false;
	if (tok->kind == tok_pp_ifdef)
	{
		tok = _pop_next();
		if (tok->kind != tok_identifier)
		{
			diag_err(tok, ERR_SYNTAX, "expected %s after #ifdef",
				tok_kind_spelling(tok_identifier));
			return false;
		}
		inc_group = _find_macro_def(tok) != NULL;
		assert(_peek_next()->flags & TF_START_LINE);
	}
	else if (tok->kind == tok_pp_ifndef)
	{
		tok = _pop_next();
		if (tok->kind != tok_identifier)
		{
			diag_err(tok, ERR_SYNTAX, "expected %s after #ifndef",
				tok_kind_spelling(tok_identifier));
			return false;
		}
		inc_group = _find_macro_def(tok) == NULL;
		assert(_peek_next()->flags & TF_START_LINE);
	}
	else if (tok->kind == tok_pp_if)
	{
		tok = _pop_next();
		if (!_eval_pp_expression(tok, &inc_group))
			return false;
	}
	else
	{
		diag_err(tok, ERR_SYNTAX, "unexpected token looking for pp conditional");
		return false;
	}

	//have we hit a true case yet?
	bool any_true = inc_group;

	while (_peek_next()->kind != tok_pp_endif)
	{
		tok = _pop_next();

		if (inc_group)
			any_true = true;

		if (tok->kind == tok_pp_elif)
		{
			inc_group = !any_true;
			if (!_eval_pp_expression(_pop_next(), &inc_group))
				return false;
		}
		else if (tok->kind == tok_pp_else)
		{
			inc_group = !any_true;
			if (_peek_next()->kind == tok_if)
			{
				tok = _pop_next();
				if (!_eval_pp_expression(_pop_next(), &inc_group))
					return false;
			}
		}
		else if (inc_group)
		{
			if (!_process_token(tok))
				return false;
		}
	}
	_pop_next();
	return true;
}

static token_t* _stringize_range(token_range_t* range)
{
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
		case tok_num_literal:
		{
			str_buff_t* sl_buff = sb_create(128);

			tok_spelling_append(tok->loc, tok->len, sl_buff);

			char* c = sl_buff->buff;

			while(*c)
			{
				// a \ character is inserted before each " and \ character of a character constant or string literal
				if (*c == '\\' || *c == '\"')
					sb_append_ch(sb, '\\');
				sb_append_ch(sb, *c);
				c++;
			}

			sb_destroy(sl_buff);
			break;
		}
		default:
			tok_spelling_append(tok->loc, tok->len, sb);
			break;
		}

		tok = tok->next;
	}
	sb_append_ch(sb, '\"');

	return _lex_single_tok(sb);
}

static token_range_t* _expand_param_range(token_range_t* range)
{
	if (tok_range_empty(range))
		return range;

	token_range_t* result = tok_range_create(NULL, NULL);

	//save and reset the expansion stack to prevent looking past the token range
	input_range_t* prev_me_stack = _context->input_stack;
	_context->input_stack = NULL;
	expansion_context_t* ec = _context->expansion_stack;
	_context->expansion_stack = NULL;

	//setup the unexpanded param tokens for expansion
	input_range_t* me = _begin_token_range_expansion(range);

	_push_dest(result);

	while (!_is_expansion_complete(me))
	{
		if (!_process_token(_pop_next()))
			return NULL;
	}

	_context->expansion_stack = ec;

	//restore the expansion stack
	_destroy_input_stack();
	_context->input_stack = prev_me_stack;

	_pop_dest();

	result->end = _create_end_marker(result->end);
	return result;
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

	//process the parameters and build a table of param name to token range
	token_t* param = macro->fn_params;
	while (tok->kind != tok_r_paren)
	{
		if (!param)
			return false; //todo error

		token_range_t* range = tok_range_create(NULL, NULL);

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

			if (tok->kind == tok_identifier)
			{
				token_range_t* p_range = _lookup_fn_param(tok->data.str);
				if (p_range)
				{
					p_range = _expand_param_range(p_range);
					tok_range_append(range, p_range);
					tok = _pop_next();
					continue;
				}
			}

			tok_range_add(range, tok);
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
			token_range_t* range = tok_range_create(NULL, NULL);

			sht_insert(params, param->data.str, range);
		}
		param = param->next;
	}

	return params;
}

static bool _should_expand_macro(token_t* identifier, macro_t* macro)
{
	if (_is_macro_expanding(macro))
		return false;
	size_t id = identifier->id;
	return !ht_contains(macro->hidden_toks, (void*)id);
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

	return _process_token_range(range);
}

static bool _process_replacement_list(token_t* replaced, macro_t* macro, hash_table_t* params)
{
	if (tok_range_empty(&macro->tokens))
		return true;

	input_range_t* ir = _begin_macro_expansion(macro, params);

	_set_next_tok_flags(replaced->flags);

	while (!_is_expansion_complete(ir))
	{
		if (!_process_token(_pop_next()))
			return false;
	}

	_pop_macro_expansion(macro);

	return true;
}

static bool _process_identifier(token_t* identifier)
{
	if (_supress_identifier_expansion(identifier))
	{
		_emit_token(identifier);
		return true;
	}

	token_t* built_in = _is_standard_macro(identifier);
	if (built_in)
	{
		_emit_token(built_in);
		return true;
	}

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

	return _process_token(tok);
}

static bool _is_valid_pasted_tok(token_t* tok)
{
	return (tok && tok->kind != tok_invalid && tok->kind != tok_eof);
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

	token_t* tok = _lex_single_tok(sb);

	if (tok && tok->kind != tok_invalid && tok->kind != tok_eof)
	{
		tok->flags = 0;

		if (lhs->flags & TF_LEADING_SPACE || rhs->flags & TF_LEADING_SPACE)
			tok->flags = TF_LEADING_SPACE;

		if (lhs->flags & TF_START_LINE)
			tok->flags |= TF_START_LINE;

		return tok;
	}
	return NULL;
}

static bool _process_hashhash(token_t* first)
{
	if (!_expect_kind(_pop_next(), tok_hashhash))
		return false;

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
				if (!_process_token(result))
					return false;

				if (!tok_range_empty(p_range))
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
	if (_context->expansion_stack && _peek_next()->kind == tok_hashhash)
	{
		return _process_hashhash(tok);
	}

	switch (tok->kind)
	{
	case tok_pp_pragma:
		if (!_process_pragma(tok))
			return false;
		break;
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

token_range_t pre_proc_file(const char* src_dir, token_range_t* range)
{
	src_dir;
	_begin_token_range_expansion(range);

	token_t* tok = _pop_next();
	while (tok->kind != tok_eof)
	{
		if (!_process_token(tok))
		{
			token_range_t result = { NULL, NULL };
			return result;
		}
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
	_context->praga_once_paths = sht_create(128);

	_push_dest(&_context->result);
}

void pre_proc_deinit()
{
	ht_destroy(_context->defs);
	ht_destroy(_context->praga_once_paths);
	free(_context);
	_context = NULL;
}