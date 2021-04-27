#pragma once

typedef enum
{
	tok_eof,
	tok_invalid,
	//punctuation
	tok_ellipse,			// ...
	tok_l_brace,            // {
	tok_r_brace,            // }
	tok_l_paren,            // (
	tok_r_paren,            // )
	tok_l_square_paren,     // [
	tok_r_square_paren,     // ]
	tok_semi_colon,         // ;
	tok_minus,              // -
	tok_minusminus,         // --
	tok_tilda,              // ~
	tok_exclaim,            // !
	tok_plus,               // +
	tok_plusplus,           // ++
	
	tok_star,               // *
	tok_slash,              // /
	tok_hash,               // #
	tok_hashhash,           // ##
	tok_amp,                // &
	tok_ampamp,             // &&
	tok_pipe,               // |
	tok_pipepipe,           // ||
	tok_equal,              // =
	tok_equalequal,         // ==
	tok_exclaimequal,       // !=
	tok_starequal,          // *=
	tok_slashequal,         // /=
	tok_percentequal,		// %=
	tok_plusequal,          // +=
	tok_minusequal,         // -=
	tok_lesserlesserequal,  // <<=
	tok_greatergreaterequal,// >>=
	tok_ampequal,			// &=
	tok_carotequal,			// ^=
	tok_pipeequal,			// |=
	tok_lesser,             // <
	tok_lesserlesser,       // <<
	tok_lesserequal,        // <=
	tok_greater,            // >
	tok_greatergreater,     // >>
	tok_greaterequal,       // >=
	tok_minusgreater,       // ->

	tok_percent,            // %
	tok_caret,              // ^
	tok_colon,              // :
	tok_question,           // ?
	tok_comma,              // ,
	tok_fullstop,           // .

	//keywords
	tok_return,
	tok_if,
	tok_else,
	tok_for,
	tok_while,
	tok_do,
	tok_break,
	tok_continue,
	tok_struct,
	tok_union,
	tok_enum,
	tok_sizeof,
	tok_switch,
	tok_case,
	tok_default,
	tok_goto,

	//type specifiers
	tok_char,
	tok_short,
	tok_int,
	tok_long,
	tok_void,
	tok_signed,
	tok_unsigned,
	tok_float,
	tok_double,

	//type qualifiers
	tok_const,
	tok_volatile,

	// storage class specifiers
	tok_typedef,
	tok_extern,
	tok_static,
	tok_auto,
	tok_register,
	tok_inline,

	// preprocessor directives
	tok_pp_null, //# \n
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
	tok_pp_endif,

	tok_pp_end_marker, //special value used interally by pre processor

	tok_identifier,         // main
	tok_num_literal,        // 1234
	tok_string_literal      // "abc"
}tok_kind;

