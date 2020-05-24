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
