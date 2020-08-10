#include "lexer.h"

#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static token_t _invalid_tok;

static inline bool _adv_pos(source_range_t* sr, const char** pos)
{
	if (*pos < sr->end) //skip the null and the last char
	{
		char c = **pos;
		(*pos)++;
		return true;
	}
	return false;
}

/*
[0-9]
*/
static inline bool _is_digit_char(const char c)
{
	return c >= '0' && c <= '9';
}

/*
[0-9A-Fa-f]
*/
static inline bool _is_hex_digit_char(const char c)
{
	return (c >= '0' && c <= '9') ||
		(c >= 'A' && c <= 'F') ||
		(c >= 'a' && c <= 'f');
}

/*
[0-7]
*/
static inline bool _is_octal_digit_char(const char c)
{
	return c >= '0' && c <= '7';
}

/*
[a-zA-Z0-9_]
*/
static inline bool _is_identifier_body(const char c)
{
	return (
		(c >= 'A' && c <= 'Z') ||
		(c >= 'a' && c <= 'z') ||
		(c == '_') ||
		_is_digit_char(c));
}

/*
[a-zA-Z_]
*/
static inline bool _is_identifier_start(const char c)
{
	return ((c >= 'A' && c <= 'Z') ||
		(c >= 'a' && c <= 'a') ||
		(c == '_'));
}

static inline int _get_char_int_val(char c)
{
	if (_is_digit_char(c))
		return c - '0';

	if (c >= 'a' && c <= 'z')
		return 10 + c - 'a';

	if (c >= 'A' && c <= 'Z')
		return 10 + c - 'A';

	return 0;
}

static inline bool _is_valid_num_suffix(char ch)
{
	return (ch == 'l' || ch == 'L' || ch == 'u' || ch == 'U');
}

static inline bool _is_valid_num_char(uint32_t base, char ch)
{
	if (base == 10)
		return _is_digit_char(ch);
	if (base == 16)
		return _is_hex_digit_char(ch);
	if (base == 8)
		return _is_octal_digit_char(ch);
	return false;
}

static void _lex_char_literal(source_range_t* sr, const char* pos, token_t* result)
{

}

static void _lex_num_literal(source_range_t* sr, const char* pos, token_t* result)
{
	result->loc = pos;
	result->len = 0;
	result->kind = tok_num_literal;
	
	uint32_t base = 10;

	if (*pos == '0')
	{
		if (!_adv_pos(sr, &pos))
		{
			result->kind = tok_eof;
			return;
		}
		if (*pos == 'x' || *pos == 'X')
		{
			//Hex
			if (!_adv_pos(sr, &pos))
			{
				result->kind = tok_eof;
				return;
			}
			base = 16;
		}
		else
		{
			//octal			
			base = 8;
		}
	}

	int i = 0;
	do
	{
		i = i * base + _get_char_int_val(*pos);
		result->len++;
		if (!_adv_pos(sr, &pos))
		{
			result->kind = tok_eof;
			return;
		}
	} while (_is_valid_num_char(base, *pos));
	result->data = (void*)(long)i;

	while (_is_valid_num_suffix(*pos))
	{
		result->len++;
		if (!_adv_pos(sr, &pos))
		{
			result->kind = tok_eof;
			return;
		}
	}
}

static void _lex_identifier(source_range_t* sr, const char* pos, token_t* result)
{
	result->loc = pos;
	result->len = 0;
	result->kind = tok_identifier;

	do
	{
		//check len
		result->len++;
		if (!_adv_pos(sr, &pos))
		{
			result->kind = tok_eof;
			return;
		}
	} while (_is_identifier_body(*pos));

	//Keywords
	if (tok_spelling_cmp(result, "int"))
		result->kind = tok_int;
	else if (tok_spelling_cmp(result, "char"))
		result->kind = tok_char;
	else if (tok_spelling_cmp(result, "short"))
		result->kind = tok_short;
	else if (tok_spelling_cmp(result, "long"))
		result->kind = tok_long;
	else if (tok_spelling_cmp(result, "void"))
		result->kind = tok_void;
	else if (tok_spelling_cmp(result, "signed"))
		result->kind = tok_signed;
	else if (tok_spelling_cmp(result, "unsigned"))
		result->kind = tok_unsigned;
	else if (tok_spelling_cmp(result, "float"))
		result->kind = tok_float;
	else if (tok_spelling_cmp(result, "double"))
		result->kind = tok_double;
	else if (tok_spelling_cmp(result, "const"))
		result->kind = tok_const;
	else if (tok_spelling_cmp(result, "volatile"))
		result->kind = tok_volatile;
	else if (tok_spelling_cmp(result, "typedef"))
		result->kind = tok_typedef;
	else if (tok_spelling_cmp(result, "extern"))
		result->kind = tok_extern;
	else if (tok_spelling_cmp(result, "static"))
		result->kind = tok_static;
	else if (tok_spelling_cmp(result, "auto"))
		result->kind = tok_auto;
	else if (tok_spelling_cmp(result, "register"))
		result->kind = tok_register;
	else if (tok_spelling_cmp(result, "return"))
		result->kind = tok_return;
	else if (tok_spelling_cmp(result, "if"))
		result->kind = tok_if;
	else if (tok_spelling_cmp(result, "else"))
		result->kind = tok_else;
	if (tok_spelling_cmp(result, "for"))
		result->kind = tok_for;
	else if (tok_spelling_cmp(result, "while"))
		result->kind = tok_while;
	else if (tok_spelling_cmp(result, "do"))
		result->kind = tok_do;
	else if (tok_spelling_cmp(result, "break"))
		result->kind = tok_break;
	else if (tok_spelling_cmp(result, "continue"))
		result->kind = tok_continue;
	else if (tok_spelling_cmp(result, "struct"))
		result->kind = tok_struct;
	else if (tok_spelling_cmp(result, "union"))
		result->kind = tok_union;
	else if (tok_spelling_cmp(result, "enum"))
		result->kind = tok_enum;
	else if (tok_spelling_cmp(result, "sizeof"))
		result->kind = tok_sizeof;
}

bool lex_next_tok(source_range_t* src, const char* pos, token_t* result)
{
	*result = _invalid_tok;

lex_next_tok:

	//Skip any white space
	while(*pos == ' ' || *pos == '\t')
	{
		if (!_adv_pos(src, &pos)) goto _hit_end;
	};

	if (*pos == '\0')
	{
		result->loc = pos;
		result->kind = tok_eof;
		result->len = 0;
		return true;
	}

	result->loc = pos;
	result->len = 0;

	switch (*pos)
	{
	case '\r':
		//fallthrough
	case '\n':
		//preprocessor
		pos++;
		goto lex_next_tok;
	case '(':
		result->kind = tok_l_paren;
		result->len = 1;
		break;
	case ')':
		result->kind = tok_r_paren;
		result->len = 1;
		break;
	case '{':
		result->kind = tok_l_brace;
		result->len = 1;
		break;
	case '}':
		result->kind = tok_r_brace;
		result->len = 1;
		break;
	case ';':
		result->kind = tok_semi_colon;
		result->len = 1;
		break;
	case '-':
		if (pos[1] == '-')
		{
			result->kind = tok_minusminus;
			result->len = 2;
		}
		else
		{
			result->kind = tok_minus;
			result->len = 1;
		}
		break;
	case '~':
		result->kind = tok_tilda;
		result->len = 1;
		break;
	case '!':
		if (pos[1] == '=')
		{
			result->kind = tok_exclaimequal;
			result->len = 2;
		}
		else
		{
			result->kind = tok_exclaim;
			result->len = 1;
		}
		break;
	case '+':
		if (pos[1] == '+')
		{
			result->kind = tok_plusplus;
			result->len = 2;
		}
		else if (pos[1] == '=')
		{
			result->kind = tok_plusequal;
			result->len = 2;
		}
		else
		{
			result->kind = tok_plus;
			result->len = 1;
		}
		break;
	case '*':
		result->kind = tok_star;
		result->len = 1;
		break;
	case '/':
		if (pos[1] == '/')
		{
			result->kind = tok_slashslash;
			result->len = 2;
		}
		else
		{
			result->kind = tok_slash;
			result->len = 1;
		}
		break;
	case '%':
		result->kind = tok_percent;
		result->len = 1;
		break;
	case '^':
		result->kind = tok_caret;
		result->len = 1;
		break;
	case ':':
		result->kind = tok_colon;
		result->len = 1;
		break;
	case '?':
		result->kind = tok_question;
		result->len = 1;
		break;
	case ',':
		result->kind = tok_comma;
		result->len = 1;
		break;
	case '.':
		result->kind = tok_fullstop;
		result->len = 1;
		break;
	case '&':
		if (pos[1] == '&')
		{
			result->kind = tok_ampamp;
			result->len = 2;
		}
		else
		{
			result->kind = tok_amp;
			result->len = 1;
		}
		break;
	case '|':
		if (pos[1] == '|')
		{
			result->kind = tok_pipepipe;
			result->len = 2;
		}
		else
		{
			result->kind = tok_pipe;
			result->len = 1;
		}
		break;
	case '=':
		if (pos[1] == '=')
		{
			result->kind = tok_equalequal;
			result->len = 2;
		}
		else
		{
			result->kind = tok_equal;
			result->len = 1;
		}
		break;
	case '<':
		if (pos[1] == '<')
		{
			result->kind = tok_lesserlesser;
			result->len = 2;
		}
		else if (pos[1] == '=')
		{
			result->kind = tok_lesserequal;
			result->len = 2;
		}
		else
		{
			result->kind = tok_lesser;
			result->len = 1;
		}
		break;
	case '>':
		if (pos[1] == '>')
		{
			result->kind = tok_greatergreater;
			result->len = 2;
		}
		else if (pos[1] == '=')
		{
			result->kind = tok_greaterequal;
			result->len = 2;
		}
		else
		{
			result->kind = tok_greater;
			result->len = 1;
		}
		break;
	case '\'':
		result->kind = tok_apostrophe;
		result->len = 1;
		_lex_char_literal(src, pos, result);
		break;
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':		
		_lex_num_literal(src, pos, result);
		break;
	// C99 6.4.2: Identifiers.
	case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G':
	case 'H': case 'I': case 'J': case 'K': case 'L': case 'M': case 'N':
	case 'O': case 'P': case 'Q': case 'R': case 'S': case 'T': case 'U':
	case 'V': case 'W': case 'X': case 'Y': case 'Z':
	case 'a': case 'b': case 'c': case 'd': case 'e': case 'f': case 'g':
	case 'h': case 'i': case 'j': case 'k': case 'l': case 'm': case 'n':
	case 'o': case 'p': case 'q': case 'r': case 's': case 't': case 'u':
	case 'v': case 'w': case 'x': case 'y': case 'z':
	case '_':
		_lex_identifier(src, pos, result);
		break;
	};

	if (result->kind == tok_slashslash)
	{
		//Skip up to eol
		do
		{
			if (!_adv_pos(src, &pos)) goto _hit_end;
		} while (*pos != '\n');
		goto lex_next_tok;
	}

	return result->kind != tok_invalid;

_hit_end:
	result->kind = tok_eof;
	return true;
}

token_t* lex_source(source_range_t* sr)
{
	const char* pos = sr->ptr;
	token_t* first = NULL;
	token_t* last = NULL;
	token_t* tok;
	do
	{
		tok = (token_t*)malloc(sizeof(token_t));
		memset(tok, 0, sizeof(token_t));

		size_t a = sizeof tok;

		if (!lex_next_tok(sr, pos, tok))
			break;

		if (first == NULL)
		{
			first = last = tok;
		}
		else
		{
			tok->prev = last;
			last->next = tok;
			last = tok;
		}
		pos = tok->loc + tok->len;
	} while (tok->kind != tok_eof);
	return first;
}

void lex_init()
{
	_invalid_tok.kind = tok_invalid;
	_invalid_tok.loc = NULL;
	_invalid_tok.len = 0;
	_invalid_tok.data = NULL;
	_invalid_tok.next = NULL;
	_invalid_tok.prev = NULL;
}