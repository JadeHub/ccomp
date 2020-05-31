#include "token.h"

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

const char* tok_kind_name(tok_kind k)
{
	switch (k)
	{
	case tok_eof:
		return "eof";
	case tok_l_brace:
		return "l_brace";
	case tok_r_brace:
		return "r_brace";
	case tok_l_paren:
		return "l_paren";
	case tok_r_paren:
		return "r_paren";
	case tok_semi_colon:
		return "semi_colon";
	case tok_return:
		return "return";
	case tok_int:
		return "int";
	case tok_identifier:
		return "identifier";
	case tok_num_literal:
		return "num_literal";
	case tok_minus:
		return "num_minus";
	case tok_tilda:
		return "num_tilda";
	case tok_exclaim:
		return "num_exclaim";
	case tok_plus:
		return "tok_plus";
	case tok_star:
		return "tok_star";
	case tok_slash:
		return "tok_slash";
	case tok_ampamp:
		return "tok_ampamp";
	case tok_pipepipe:
		return "tok_pipepipe";
	case tok_equalequal:
		return "tok_equalequal";
	case tok_exclaimequal:
		return "tok_exclaimequal";
	case tok_lesser:
		return "tok_lesser";
	case tok_lesserequal:
		return "tok_lesserequal";
	case tok_greater:
		return "tok_greater";
	case tok_greaterequal:
		return "tok_greaterequal";
	case tok_percent:
		return "tok_percent";
	case tok_caret:
		return "tok_caret";
	case tok_colon:
		return "tok_colon";
	case tok_question:
		return "tok_question";
	case tok_if:
		return "tok_if";
	case tok_else:
		return "tok_else";
	case tok_for:
		return "tok_for";
	case tok_while:
		return "tok_while";
	case tok_do:
		return "tok_do";
	case tok_break:
		return "tok_break";
	case tok_continue:
		return "tok_continue";
	case tok_comma:
		return "tok_comma";
	}
	return "invalid";
}

void tok_printf(token_t* tok)
{
	if (tok->kind == tok_num_literal)
	{
		printf("Tok %s %d\n", tok_kind_name(tok->kind), (uint32_t)tok->data);
	}
	else if (tok->kind == tok_identifier)
	{
		char buff[32];
		assert(tok->len < 32);
		strncpy(buff, tok->loc, tok->len);
		buff[tok->len] = '\0';
		printf("Tok %s %s\n", tok_kind_name(tok->kind), buff);
	}
	else
	{
		printf("Tok %s\n", tok_kind_name(tok->kind));
	}
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
