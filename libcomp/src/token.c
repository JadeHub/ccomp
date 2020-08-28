#include "token.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

const char* tok_kind_spelling(tok_kind k)
{
	switch (k)
	{
		case tok_eof: return "";
	}

	switch (k)
	{
	case tok_eof:
		return "eof";
	case tok_l_brace:
		return "(";
	case tok_r_brace:
		return ")";
	case tok_l_paren:
		return "{";
	case tok_r_paren:
		return "}";
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
	}
	return "invalid";
}

void tok_printf(token_t* tok)
{
	if (tok->kind == tok_num_literal)
	{
		printf("Tok %s %d\n", tok_kind_spelling(tok->kind), (uint32_t)(long)tok->data);
	}
	else if (tok->kind == tok_identifier)
	{
		char buff[32];
		assert(tok->len < 32);
		strncpy(buff, tok->loc, tok->len);
		buff[tok->len] = '\0';
		printf("Tok %s %s\n", tok_kind_spelling(tok->kind), buff);
	}
	else
	{
		printf("Tok %s\n", tok_kind_spelling(tok->kind));
	}
}

size_t tok_spelling_len(token_t* tok)
{
	return tok->len;
}

void tok_spelling_cpy(token_t* tok, char* dest, size_t len)
{
	assert(len > tok->len);
	strncpy(dest, tok->loc, tok->len);
	dest[tok->len] = '\0';
}

bool tok_spelling_cmp(token_t* tok, const char* str)
{
	size_t l = strlen(str);
	if (tok->len > l)
		return false;
	return strncmp(tok->loc, str, l) == 0;
}

bool tok_is_in_range(token_t* tok, token_range_t* range)
{
	if (!tok_is_valid_range(range)) return false;
	return tok->loc >= range->start->loc &&
		tok->loc + tok->len <= range->end->loc + range->end->len;
}

size_t tok_range_len(token_range_t* range)
{
	if (!tok_is_valid_range(range))
		return 0;
	return range->end->loc + range->end->len - range->start->loc;
}

bool tok_is_valid_range(token_range_t* range)
{
	return range->start && range->end;
}


