#pragma once

typedef enum
{
	tok_pp_include,
	tok_pp_define,
	tok_pp_undef,
	tok_pp_line,
	tok_pp_error,
	tok_pp_pragma,
	tok_pp_if,
	tok_pp_ifdef,
	tok_pp_ifndef,
	tok_pp_else,
	tok_pp_elif,
	tok_pp_endif
}tok_pp_kind;