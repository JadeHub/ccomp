#include "token.h"

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
	case tok_slashslash:
		return "//";
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

void tok_printf(token_t* tok)
{
	if (tok->kind == tok_num_literal)
	{
		printf("Tok %s %d\n", tok_kind_spelling(tok->kind), (uint32_t)(long)tok->data);
	}
	else if (tok->kind == tok_string_literal)
	{
		printf("Tok %s \"%s\"\n", tok_kind_spelling(tok->kind), (const char*)tok->data);
	}
	else if (tok->kind == tok_identifier)
	{
		char buff[33];
		tok_spelling_cpy(tok, buff, 33);
		printf("Tok %s %s\n", tok_kind_spelling(tok->kind), buff);
	}
	else
	{
		printf("Tok %s\n", tok_kind_spelling(tok->kind));
	}
}

void tok_dump_range(token_t* start, token_t* end)
{
	while (start)
	{
		tok_printf(start);
		if (start == end)
			break;
		start = start->next;
	}
}

void tok_dump(token_t* tok)
{
	while (tok)
	{
		tok_printf(tok);
		tok = tok->next;
	}
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

void tok_spelling_extract(const char* src_loc, size_t src_len, char* dest, size_t dest_len)
{
	const char* src = src_loc;
	int pos = 0;
	while ((src < src_loc+src_len) && (pos < dest_len - 1))
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
		dest[pos] = *src;
		pos++;
		src++;
	}
	dest[pos] = '\0';
}

void tok_spelling_cpy(token_t* tok, char* dest, size_t dest_len)
{
	tok_spelling_extract(tok->loc, tok->len, dest, dest_len);
}

const char* tok_spelling_dup(token_t* tok)
{
	size_t l = tok_spelling_len(tok);
	if (l == 0) return NULL;
	char* buff = (char*)malloc(l + 1);
	tok_spelling_cpy(tok, buff, l + 1);
	return buff;
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

size_t tok_range_len(token_range_t* range)
{
	token_t* tok = range->start;
	size_t len = 0;

	while (tok && tok != range->end)
	{
		len++;
		tok = tok->next;
	}
	return len;
}

void tok_destory(token_t* tok)
{
	if (tok && tok->kind == tok_string_literal)
	{
		free((char*)tok->data);
	}
	free(tok);
}

void tok_destroy_range(token_range_t* range)
{
	token_t* tok = range->start;

	while (tok && tok != range->end)
	{
		token_t* next = tok->next;
		tok_destory(tok);
		tok = next;
	}
}