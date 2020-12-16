#include "lexer.h"
#include "diag.h"

#include <libj/include/str_buff.h>
#include <libj/include/byte_buff.h>

#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

static inline bool _can_adv(source_range_t* sr, const char** pos)
{
	return *pos < sr->end;
}

static inline bool _adv_pos(source_range_t* sr, const char** pos)
{
	if (_can_adv(sr, pos))
	{
		char c = **pos;

		(*pos)++;
		c = **pos;

		if (c == '\\')
		{
			if (*(*pos + 1) == '\n')
			{
				(*pos) += 2;
			}
			else if (*(*pos + 1) == '\r' && *(*pos + 2) == '\n')
			{
				(*pos) += 3;
			}
		}

		return true;
	}
	return false;
}

static inline char _peek_next(source_range_t* sr, const char* pos)
{
	if (_can_adv(sr, &pos))
	{
		pos++;
		char c = *pos;

		if (c == '\\')
		{
			if (*(pos + 1) == '\n')
			{
				(pos) += 2;
			}
			else if (*(pos + 1) == '\r' && *(pos + 2) == '\n')
			{
				(pos) += 3;
			}
		}
		return *pos;
	}
	return 0;
}

#define ADV_POS(SR, POS)	if (!_adv_pos(SR, POS)) \
							{	result->kind = tok_eof; return; }

#define ADV_POS_ERR(TOK, SR, POS)	if (!_adv_pos(SR, POS)) \
									{ diag_err(TOK, ERR_SYNTAX, "Unexpected EOF"); result->kind = tok_eof; return; }

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

static inline bool _is_white_space(char ch)
{
	return ch == ' ' ||
		ch == '\t' ||
		ch == '\n' ||
		ch == '\r';
}

static uint8_t _lex_escaped_char(source_range_t* sr, token_t* tok, const char* pos, int* len)
{
	//skip the slash
	if (!_adv_pos(sr, &pos))
		goto esc_err_ret;
	*len = 1;

	int base = 0;
	if (*pos == 'x')
	{
		//hex
		if (!_adv_pos(sr, &pos))
			goto esc_err_ret;

		(*len)++;
		base = 16;
	}
	else if (_is_octal_digit_char(*pos))
	{
		//octal
		base = 8;
	}

	if (base > 0)
	{
		int i = 0;
		do
		{
			i = i * base + _get_char_int_val(*pos);
			(*len)++;
			if (!_adv_pos(sr, &pos))
				goto esc_err_ret;
		} while (_is_valid_num_char(base, *pos));

		if (i > 255)
		{
			diag_err(tok, ERR_SYNTAX, "Hex/Octal literal too large %d", i);
			goto esc_err_ret;
		}
		return (uint8_t)i;
	}

	switch (*pos)
	{
	case '\\':
	case '\'':
	case '\"':
	case '?':
		(*len)++;
		return  *pos;
	case 'a':
		(*len)++;
		return 0x07; //bell;
	case 'b':
		(*len)++;
		return 0x08; //backspace
	case 'f':
		(*len)++;
		return 0x0C; //form feed
	case 'n':
		(*len)++;
		return 0x0A; //new line
	case 'r':
		(*len)++;
		return 0x0D; //carriage return
	case 't':
		(*len)++;
		return 0x09; //tab
	case 'v':
		(*len)++;
		return 0x0B; //vert tab
	case '0':
		(*len)++;
		return 0;
	default:
		(*len)++;
		diag_err(tok, ERR_SYNTAX, "Unrecognised escape sequence %c", *pos);
		break;
	}

esc_err_ret:
	return 0;
}

static inline bool _is_valid_string_literal_char(char c)
{
	return c != '\r' && c != '\n';
}

static void _lex_string_literal(source_range_t* sr, const char* pos, token_t* result, const char end_char)
{
	result->loc = pos;
	result->len = 1;
	result->kind = tok_string_literal;

	byte_buff_t* bb = bb_create(128);

	//skip initial quote
	ADV_POS_ERR(result, sr, &pos);

	while (*pos != end_char)
	{
		if (!_is_valid_string_literal_char(*pos))
		{
			diag_err(result, ERR_SYNTAX, "Unterminated string literal");
			result->kind = tok_eof;
			return;
		}
		if (*pos == '\\')
		{
			int len;
			uint8_t val = _lex_escaped_char(sr, result, pos, &len);
			if (len == -1)
				return;
			bb_append(bb, val);
			pos += len;
		}
		else
		{
			bb_append(bb, *pos);
			ADV_POS_ERR(result, sr, &pos);
		}
	}
	//skip closing quote
	ADV_POS_ERR(result, sr, &pos);

	bb_append(bb, 0);
	
	result->len = pos - result->loc;
	result->data.str = (char*)bb_release(bb);
}

static void _lex_char_literal(source_range_t* sr, const char* pos, token_t* result)
{
	result->loc = pos;
	result->len = 1;
	result->kind = tok_num_literal;

	//skip initial apostrophe
	ADV_POS_ERR(result, sr, &pos);
	 
	if (*pos == '\\')
	{
		int len;
		uint8_t val = _lex_escaped_char(sr, result, pos, &len);
		result->data.int_val = int_val_unsigned(val);
		result->len = len;
		pos += result->len;
	}
	else if (*pos == '\'')
	{
		diag_err(result, ERR_SYNTAX, "Empty char literal");
		result->kind = tok_eof;
		return;
	}
	else
	{
		uint8_t val = *pos;
		result->data.int_val = int_val_unsigned(val);
		ADV_POS_ERR(result, sr, &pos);
	}

	if (*pos != '\'')
	{
		diag_err(result, ERR_SYNTAX, "Char literal too long");
		result->kind = tok_eof;
		return;
	}
	//slip trailing apostrophe
	ADV_POS_ERR(result, sr, &pos);
	result->len = pos - result->loc;
}

static void _lex_num_literal(source_range_t* sr, const char* pos, token_t* result)
{
	result->loc = pos;
	result->len = 0;
	result->kind = tok_num_literal;
	
	uint32_t base = 10;

	if (*pos == '0')
	{		
		if (*(pos+1) == 'x' || *(pos+1) == 'X')
		{
			//Hex
			ADV_POS_ERR(result, sr, &pos); //skip the '0'
			ADV_POS_ERR(result, sr, &pos); //skip the 'x'
			base = 16;
		}
		else
		{
			//octal			
			base = 8;
		}
	}

	uint32_t i = 0;
	do
	{
		i = i * base + _get_char_int_val(*pos);
		ADV_POS(sr, &pos);
	} while (_is_valid_num_char(base, *pos));

	result->data.int_val = int_val_unsigned(i);

	while (_is_valid_num_suffix(*pos))
		ADV_POS(sr, &pos);
	result->len = pos - result->loc;
}

static void _lex_identifier(source_range_t* sr, const char* pos, token_t* result)
{
	result->loc = pos;
	result->len = 0;
	result->kind = tok_identifier;

	do
	{
		ADV_POS(sr, &pos);
	} while (_is_identifier_body(*pos));
	result->len = pos - result->loc;

	str_buff_t* sb = sb_create(64);
	tok_spelling_append(result->loc, result->len, sb);

	//Keywords
	if (strcmp(sb->buff, "int") == 0)
		result->kind = tok_int;
	else if (strcmp(sb->buff, "char") == 0)
		result->kind = tok_char;
	else if (strcmp(sb->buff, "short") == 0)
		result->kind = tok_short;
	else if (strcmp(sb->buff, "long") == 0)
		result->kind = tok_long;
	else if (strcmp(sb->buff, "void") == 0)
		result->kind = tok_void;
	else if (strcmp(sb->buff, "signed") == 0)
		result->kind = tok_signed;
	else if (strcmp(sb->buff, "unsigned") == 0)
		result->kind = tok_unsigned;
	else if (strcmp(sb->buff, "float") == 0)
		result->kind = tok_float;
	else if (strcmp(sb->buff, "double") == 0)
		result->kind = tok_double;
	else if (strcmp(sb->buff, "const") == 0)
		result->kind = tok_const;
	else if (strcmp(sb->buff, "volatile") == 0)
		result->kind = tok_volatile;
	else if (strcmp(sb->buff, "typedef") == 0)
		result->kind = tok_typedef;
	else if (strcmp(sb->buff, "extern") == 0)
		result->kind = tok_extern;
	else if (strcmp(sb->buff, "static") == 0)
		result->kind = tok_static;
	else if (strcmp(sb->buff, "auto") == 0)
		result->kind = tok_auto;
	else if (strcmp(sb->buff, "register") == 0)
		result->kind = tok_register;
	else if (strcmp(sb->buff, "return") == 0)
		result->kind = tok_return;
	else if (strcmp(sb->buff, "if") == 0)
		result->kind = tok_if;
	else if (strcmp(sb->buff, "else") == 0)
		result->kind = tok_else;
	else if (strcmp(sb->buff, "for") == 0)
		result->kind = tok_for;
	else if (strcmp(sb->buff, "while") == 0)
		result->kind = tok_while;
	else if (strcmp(sb->buff, "do") == 0)
		result->kind = tok_do;
	else if (strcmp(sb->buff, "break") == 0)
		result->kind = tok_break;
	else if (strcmp(sb->buff, "continue") == 0)
		result->kind = tok_continue;
	else if (strcmp(sb->buff, "struct") == 0)
		result->kind = tok_struct;
	else if (strcmp(sb->buff, "union") == 0)
		result->kind = tok_union;
	else if (strcmp(sb->buff, "enum") == 0)
		result->kind = tok_enum;
	else if (strcmp(sb->buff, "sizeof") == 0)
		result->kind = tok_sizeof;
	else if (strcmp(sb->buff, "switch") == 0)
		result->kind = tok_switch;
	else if (strcmp(sb->buff, "case") == 0)
		result->kind = tok_case;
	else if (strcmp(sb->buff, "default") == 0)
		result->kind = tok_default;
	else if (strcmp(sb->buff, "goto") == 0)
		result->kind = tok_goto;
	
	if (result->kind == tok_identifier)
		result->data.str = sb_release(sb);
	else
		sb_destroy(sb);
}

static void _lex_pre_proc_directive(source_range_t* sr, const char* pos, token_t* result)
{
	result->loc = pos;
	result->len = 0;

	do
	{
		ADV_POS(sr, &pos);
	} while (_is_identifier_body(*pos) || *pos == '#');
	result->len = pos - result->loc;

	str_buff_t* sb = sb_create(64);
	tok_spelling_append(result->loc + 1, result->len - 1, sb);

	if (result->len == 1)
		result->kind = tok_pp_null;
	else if (strcmp(sb->buff, "include") == 0)
		result->kind = tok_pp_include;
	else if (strcmp(sb->buff, "define") == 0)
		result->kind = tok_pp_define;
	else if (strcmp(sb->buff, "undef") == 0)
		result->kind = tok_pp_undef;
	else if (strcmp(sb->buff, "line") == 0)
		result->kind = tok_pp_line;
	else if (strcmp(sb->buff, "error") == 0)
		result->kind = tok_pp_error;
	else if (strcmp(sb->buff, "pragma") == 0)
		result->kind = tok_pp_pragma;
	else if (strcmp(sb->buff, "if") == 0)
		result->kind = tok_pp_if;
	else if (strcmp(sb->buff, "ifdef") == 0)
		result->kind = tok_pp_ifdef;
	else if (strcmp(sb->buff, "ifndef") == 0)
		result->kind = tok_pp_ifndef;
	else if (strcmp(sb->buff, "else") == 0)
		result->kind = tok_pp_else;
	else if (strcmp(sb->buff, "elif") == 0)
		result->kind = tok_pp_elif;
	else if (strcmp(sb->buff, "endif") == 0)
		result->kind = tok_pp_endif;
	else
	{
		diag_err(result, ERR_SYNTAX, "unknown preprocessor directive %s", sb->buff);
		result->kind = tok_eof;
	}
	sb_destroy(sb);
}

bool lex_next_tok(source_range_t* src, const char* pos, token_t* result)
{
lex_next_tok:

	//Skip any white space
	while(*pos == ' ' || *pos == '\t')
	{
		result->flags |= TF_LEADING_SPACE;
		if (!_adv_pos(src, &pos)) goto _hit_end;
	};

	if (*pos == '\0')
	{
		//end of file
		result->loc = pos;
		result->kind = tok_eof;
		result->len = 0;
		return true;
	}

	result->loc = pos;
	result->len = 0;

	bool slashslash = false;
	bool slashstar = false;

	switch (*pos)
	{
	case '\r':
		//fallthrough
	case '\n':
		//Skip newline, mark token flags as starting of line and lex next token
		pos++;
		result->flags = TF_START_LINE;
		goto lex_next_tok;
	case '(':
		_adv_pos(src, &pos);
		result->kind = tok_l_paren;
		break;
	case ')':
		_adv_pos(src, &pos);
		result->kind = tok_r_paren;
		break;
	case '{':
		_adv_pos(src, &pos);
		result->kind = tok_l_brace;
		break;
	case '}':
		_adv_pos(src, &pos);
		result->kind = tok_r_brace;
		break;
	case '[':
		_adv_pos(src, &pos);
		result->kind = tok_l_square_paren;
		break;
	case ']':
		_adv_pos(src, &pos);
		result->kind = tok_r_square_paren;
		break;
	case ';':
		_adv_pos(src, &pos);
		result->kind = tok_semi_colon;
		break;
	case '-':
		_adv_pos(src, &pos);
		if (*pos == '-')
		{
			result->kind = tok_minusminus;
			_adv_pos(src, &pos);
		}
		else if (*pos == '>')
		{
			result->kind = tok_minusgreater;
			_adv_pos(src, &pos);
		}
		else
		{
			result->kind = tok_minus;
		}
		break;
	case '~':
		_adv_pos(src, &pos);
		result->kind = tok_tilda;
		break;
	case '!':
		_adv_pos(src, &pos);
		if (*pos == '=')
		{
			_adv_pos(src, &pos);
			result->kind = tok_exclaimequal;
		}
		else
		{
			result->kind = tok_exclaim;
		}
		break;
	case '+':
		_adv_pos(src, &pos);
		if (*pos == '+')
		{
			_adv_pos(src, &pos);
			result->kind = tok_plusplus;
		}
		else if (*pos == '=')
		{
			_adv_pos(src, &pos);
			result->kind = tok_plusequal;
		}
		else
		{
			result->kind = tok_plus;
		}
		break;
	case '*':
		_adv_pos(src, &pos);
		result->kind = tok_star;
		break;
	case '/':
		_adv_pos(src, &pos);
		if (*pos == '/')
		{
			_adv_pos(src, &pos);
			slashslash = true;
		}
		else if (*pos == '*')
		{
			_adv_pos(src, &pos);
			slashstar = true;
		}
		else
		{
			result->kind = tok_slash;
		}
		break;
	case '%':
		_adv_pos(src, &pos);
		result->kind = tok_percent;
		break;
	case '^':
		_adv_pos(src, &pos);
		result->kind = tok_caret;
		break;
	case ':':
		_adv_pos(src, &pos);
		result->kind = tok_colon;
		break;
	case '?':
		_adv_pos(src, &pos);
		result->kind = tok_question;
		break;
	case ',':
		_adv_pos(src, &pos);
		result->kind = tok_comma;
		break;
	case '.':
		_adv_pos(src, &pos);
		result->kind = tok_fullstop;
		break;
	case '&':
		_adv_pos(src, &pos);
		if (*pos == '&')
		{
			_adv_pos(src, &pos);
			result->kind = tok_ampamp;
		}
		else
		{
			result->kind = tok_amp;
		}
		break;
	case '|':
		_adv_pos(src, &pos);
		if (*pos == '|')
		{
			_adv_pos(src, &pos);
			result->kind = tok_pipepipe;
		}
		else
		{
			result->kind = tok_pipe;
		}
		break;
	case '=':
		_adv_pos(src, &pos);
		if (*pos == '=')
		{
			_adv_pos(src, &pos);
			result->kind = tok_equalequal;
		}
		else
		{
			result->kind = tok_equal;
		}
		break;
	case '<':
		_adv_pos(src, &pos);
		if (*pos == '<')
		{
			_adv_pos(src, &pos);
			result->kind = tok_lesserlesser;
		}
		else if (*pos == '=')
		{
			_adv_pos(src, &pos);
			result->kind = tok_lesserequal;
		}
		else
		{
			result->kind = tok_lesser;
		}
		break;
	case '>':
		_adv_pos(src, &pos);
		if (*pos == '>')
		{
			_adv_pos(src, &pos);
			result->kind = tok_greatergreater;
		}
		else if (*pos == '=')
		{
			_adv_pos(src, &pos);
			result->kind = tok_greaterequal;
		}
		else
		{
			result->kind = tok_greater;
		}
		break;
	case '\'':
		_lex_char_literal(src, pos, result);
		break;
	case '\"':
		_lex_string_literal(src, pos, result, '\"');
		break;

	case '#':
		if (result->flags & TF_START_LINE)
		{
			_lex_pre_proc_directive(src, pos, result);
		}
		else
		{
			_adv_pos(src, &pos);
			if (*pos == '#')
			{
				_adv_pos(src, &pos);
				result->kind = tok_hashhash;
			}
			else
			{
				result->kind = tok_hash;
			}
		}
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
	default:
		{
			//unknown tok
			//we allow unknown tokens here as they may eventually
			//be excluded from compilation by the pre processor
			_adv_pos(src, &pos);
			result->kind = tok_invalid;
		}
	};

	if (slashslash)
	{
		//Skip up to eol
		do
		{
			if (!_adv_pos(src, &pos)) goto _hit_end;
		} while (*pos != '\n');

		//replace with single space
		result->flags |= TF_LEADING_SPACE;
		goto lex_next_tok;
	}

	if (slashstar)
	{
		/**/
		bool star = false;
		while(1)
		{
			if (!_adv_pos(src, &pos))
			{
				diag_err(result, ERR_SYNTAX, "unterminated comment");
				goto _hit_end;
			}

			if (star && *pos == '/')
			{
				_adv_pos(src, &pos);
				break;
			}
			star = *pos == '*';
		} ;

		//replace with single space
		result->flags |= TF_LEADING_SPACE;
		goto lex_next_tok;
	}

	if(result->len == 0)
		result->len = pos - result->loc;

	return result->kind != tok_invalid;

_hit_end:
	result->kind = tok_eof;
	return true;
}

token_range_t lex_source(source_range_t* sr)
{
	const char* pos = sr->ptr;
	token_range_t result = { NULL, NULL };
	token_t* tok;
	do
	{
		tok = tok_create();
		tok->kind = tok_invalid;

		if (result.start == NULL)
		{
			tok->flags |= TF_START_LINE;
			result.start = result.end = tok;
		}
		else
		{
			tok->prev = result.end;
			result.end->next = tok;
		}

		lex_next_tok(sr, pos, tok);

		if (tok->kind == tok_eof && tok->loc < sr->end-1)
			goto return_err;

		if (tok->kind == tok_eof)
		{
			//enfore newline before eol
			tok->flags |= TF_START_LINE;
		}

		result.end = tok;
		pos = tok->loc + tok->len;
	} while (tok->kind != tok_eof);
	return result;

return_err:

	tok = result.start;
	while (tok)
	{
		token_t* next = tok->next;
		free(tok);
		tok = next;
	}
	result.start = result.end = NULL;
	return result;
}

void lex_init()
{
	
}