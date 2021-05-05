#pragma once

//diagnostics

#include <stdint.h>
#include <stdarg.h>

#define ERR_SYNTAX 1
#define ERR_UNKNOWN_TYPE 2
#define ERR_FUNC_DUP_BODY 3			//Multiple definitions of the same function
#define ERR_FUNC_DIFF_PARAMS 4		//Multiple declarations with different param lists
#define ERR_DUP_VAR 5
//6
#define ERR_UNKNOWN_IDENTIFIER 7
#define ERR_INVALID_PARAMS 8
#define ERR_DUP_SYMBOL 9	//either function or global var
#define ERR_INVALID_INIT 10 //invalid initialisation
#define ERR_DUP_TYPE_DEF 11	//Multiple definitions of the same struct
#define ERR_TYPE_INCOMPLETE 11	//Type not defined
#define ERR_INVALID_RETURN 12 //return statmement is not valid
#define ERR_INCOMPATIBLE_TYPE 13
#define ERR_UNKNOWN_MEMBER_REF 14
#define ERR_UNSUPPORTED 15
#define ERR_INVALID_SWITCH 16 //switch statmement is not valid
#define ERR_DUP_LABEL 17
#define ERR_UNKNOWN_LABEL 18
#define ERR_UNKNOWN_SRC_FILE 19
#define ERR_INITIALISER_NOT_CONST 20
#define ERR_VALUE_OVERFLOW 21
#define ERR_UNKNOWN 22 //compiler error


struct token;

/*
callback used to indicate errors

tok - token at which error occured
err - error constant from above
msg - error text
data - callback context data
*/
typedef void (*diag_cb)(struct token* tok, uint32_t err, const char* msg, void* data);

/*
set callback and context data
*/
void diag_set_handler(diag_cb, void* data);

/*
generate an error
tok - token at which error occured
err - error constant from above
format, ... - error text
*/
void* diag_err(struct token* tok, uint32_t err, const char* format, ...);

/*
return a description for a token suitable for inserting into a error message
*/
const char* diag_tok_desc(struct token*);