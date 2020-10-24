#pragma once

#include <stdint.h>
#include <stdarg.h>

#define ERR_SYNTAX 1
#define ERR_UNKNOWN_TYPE 2
#define ERR_FUNC_DUP_BODY 3			//Multiple definitions of the same function
#define ERR_FUNC_DIFF_PARAMS 4		//Multiple declarations with different param lists
#define ERR_DUP_VAR 5
#define ERR_UNKNOWN_VAR 6
#define ERR_UNKNOWN_FUNC 7
#define ERR_INVALID_PARAMS 8
#define ERR_DUP_SYMBOL 9	//either function or global var
#define ERR_INVALID_INIT 10 //global must have const init
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

struct token;

typedef void (*diag_cb)(struct token* toc, uint32_t err, const char* msg, void* data);

void diag_set_handler(diag_cb, void* data);
void* diag_err(struct token* tok, uint32_t err, const char* format, ...);
const char* diag_pos_str(struct token*);
const char* diag_tok_desc(struct token*);