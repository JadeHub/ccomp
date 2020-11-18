#include "token.h"

#include <libj/include/str_buff.h>

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

const char* tok_kind_spelling(tok_kind k)
{
	switch (k)
	{
	case tok_eof:
		return "eof";
	case tok_l_brace:
		return "{";
	case tok_r_brace:
		return "}";
	case tok_l_paren:
		return "(";
	case tok_r_paren:
		return ")";
	case tok_l_square_paren:
		return "[";
	case tok_r_square_paren:
		return "]";
	case tok_semi_colon:
		return ";";
	case tok_return:
		return "\r";
	case tok_char:
		return "char";
	case tok_short:
		return "short";
	case tok_int:
		return "int";
	case tok_long:
		return "long";
	case tok_identifier:
		return "identifier";
	case tok_num_literal:
		return "num_literal";
	case tok_string_literal:
		return "string_literal";
	case tok_minus:
		return "-";
	case tok_tilda:
		return "~";
	case tok_exclaim:
		return "!";
	case tok_plus:
		return "+";
	case tok_star:
		return "*";
	case tok_slash:
		return "/";
	case tok_ampamp:
		return "&&";
	case tok_pipepipe:
		return "||";
	case tok_equalequal:
		return "==";
	case tok_exclaimequal:
		return "!=";
	case tok_lesser:
		return "<";
	case tok_lesserequal:
		return "<=";
	case tok_greater:
		return ">";
	case tok_greaterequal:
		return ">=";
	case tok_percent:
		return "%";
	case tok_caret:
		return "^";
	case tok_colon:
		return ":";
	case tok_question:
		return "?";
	case tok_if:
		return "if";
	case tok_else:
		return "else";
	case tok_for:
		return "for";
	case tok_while:
		return "while";
	case tok_do:
		return "do";
	case tok_break:
		return "break";
	case tok_continue:
		return "continue";
	case tok_struct:
		return "struct";
	case tok_union:
		return "union";
	case tok_enum:
		return "enum";
	case tok_sizeof:
		return "sizeof";
	case tok_switch:
		return "switch";
	case tok_case:
		return "case";
	case tok_default:
		return "default";
	case tok_goto:
		return "goto";
	case tok_comma:
		return ",";
	case tok_void:
		return "void";
	case tok_fullstop:
		return ".";
		//	case tok_apostrophe:
			//	return "'";
	case tok_signed:
		return "signed";
	case tok_unsigned:
		return "unsigned";
	case tok_float:
		return "float";
	case tok_double:
		return "double";
	case tok_const:
		return "const";
	case tok_volatile:
		return "volatile";
	case tok_typedef:
		return "typedef";
	case tok_extern:
		return "extern";
	case tok_static:
		return "static";
	case tok_auto:
		return "auto";
	case tok_register:
		return "register";
	case tok_minusgreater:
		return "minusgreater";
	case tok_pp_include:
		return "#include";
	case tok_pp_define:
		return "#define";
	case tok_pp_undef:
		return "#undef";
	case tok_pp_line:
		return "#line";
	case tok_pp_error:
		return "#error";
	case tok_pp_pragma:
		return "#pragma";
	case tok_pp_if:
		return "#if";
	case tok_pp_ifdef:
		return "#ifdef";
	case tok_pp_ifndef:
		return "#ifndef";
	case tok_pp_else:
		return "#else";
	case tok_pp_elif:
		return "#elif";
	case tok_pp_endif:
		return "#endif";
	case  tok_hashhash:
		return "##";
	case  tok_hash:
		return "#";
	}
	return "invalid";
}

size_t tok_spelling_len(token_t* tok)
{
	size_t len = 0;
	const char* src = tok->loc;

	while (src < (tok->loc + tok->len))
	{
		if (*src == '\\')
		{
			if (src[1] == '\n')
			{
				src += 2;
				continue;
			}
			if (src[1] == '\r' && src[2] == '\n')
			{
				src += 3;
				continue;
			}
		}
		len++;
		src++;
	}
	return len;
}

void tok_spelling_append(const char* src_loc, size_t src_len, str_buff_t* result)
{
	const char* src = src_loc;
	while (src < src_loc + src_len)
	{
		if (*src == '\\')
		{
			if (src[1] == '\n')
			{
				src += 2;
				continue;
			}
			if (src[1] == '\r' && src[2] == '\n')
			{
				src += 3;
				continue;
			}
		}
		sb_append_ch(result, *src);
		
		src++;
	}
}

void tok_spelling_cpy(token_t* tok, char* dest, size_t dest_len)
{
	if (tok->kind == tok_identifier || tok->kind == tok_string_literal)
	{
		strncpy(dest, tok->data.str, dest_len);
	}
	else
	{
		dest[0] = '\0';
		str_buff_t* sb = sb_attach(dest, dest_len);

		tok_spelling_append(tok->loc, tok->len, sb);
		sb_release(sb);
	}
}

token_t* tok_find_next(token_t* start, tok_kind kind)
{
	token_t* tok = start;

	while (tok)
	{
		if (tok->kind == kind)
			return tok;
		tok = tok->next;
	}

	return NULL;
}

void tok_destory(token_t* tok)
{
	if (!tok) return;
	if (tok->kind == tok_string_literal || tok->kind == tok_identifier)
	{
		free(tok->data.str);
	}
	free(tok);
}


/*
Two replacement lists are identical if and only if the preprocessing tokens in both have
the same number, ordering, spelling, and white-space separation, where all white-space
separations are considered identical.
*/
bool tok_equals(token_t* lhs, token_t* rhs)
{
	bool result = (lhs && rhs) &&
		lhs->kind == rhs->kind &&
		lhs->flags == rhs->flags; //?
	if (!result)
		return false;

	if (lhs->kind == tok_identifier || lhs->kind == tok_string_literal)
		result = strcmp(lhs->data.str, rhs->data.str) == 0;		
	else if (lhs->kind == tok_num_literal)
		result = lhs->data.integer == rhs->data.integer;
	return result;
}

bool tok_range_equals(token_range_t* lhr, token_range_t* rhr)
{
	token_t* lhs = lhr->start;
	token_t* rhs = rhr->start;

	while (lhs != lhr->end && rhs != rhr->end)
	{
		if (!tok_equals(lhs, rhs))
			return false;

		lhs = lhs->next;
		rhs = rhs->next;
	}

	return lhs == lhr->end && rhs == rhr->end;
}

token_t* tok_duplicate(token_t* tok)
{
	token_t* result = (token_t*)malloc(sizeof(token_t));
	*result = *tok;
	return result;
}

void tok_printf(token_t* tok)
{
	if (tok->flags & TF_START_LINE)
		printf("\n");
	if (tok->flags & TF_LEADING_SPACE)
		printf(" ");

	str_buff_t* sb = sb_create(128);

	tok_spelling_append(tok->loc, tok->len, sb);

	printf("%s", sb->buff);

	sb_destroy(sb);
}

void tok_range_print(token_range_t* range)
{
	token_t* tok = range->start;

	while (tok != range->end)
	{
		tok_printf(tok);
		tok = tok->next;
	}
	printf("\n");
}

static uint32_t _next = 1;

token_t* tok_create()
{
	token_t* tok = (token_t*)malloc(sizeof(token_t));
	memset(tok, 0, sizeof(token_t));

	tok->id = _next;
	_next++;

	return tok;
}

token_range_t* tok_range_create(token_t* start, token_t* end)
{
	token_range_t* range = (token_range_t*)malloc(sizeof(token_range_t));
	range->start = start;
	range->end = end;
	return range;
}

void tok_range_destroy(token_range_t* range)
{
	token_t* tok = range->start;

	while(tok)
	{
		token_t* next = tok->next;
		tok_destory(tok);

		if (tok == range->end)
			break;
		tok = next;
	};
}

bool tok_range_empty(token_range_t* range)
{
	return range->start == range->end;
}
