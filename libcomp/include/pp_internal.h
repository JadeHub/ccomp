#pragma once

#include "token.h"

#include <libj/include/hash_table.h>

typedef struct
{
	/*
	Map of identifier string to token_range_t*
	*/
	hash_table_t* defs;
	token_t* first;
	token_t* last;
}pp_context_t;

bool pre_proc_eval_expr(pp_context_t* pp, token_range_t range, uint32_t* val);