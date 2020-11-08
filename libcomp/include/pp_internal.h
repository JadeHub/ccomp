#pragma once

#include "token.h"

#include <libj/include/hash_table.h>

typedef enum
{
	macro_obj,
	macro_fn
}macro_kind;

typedef struct
{
	macro_kind kind;
	token_range_t tokens;
	token_t* fn_params;
	bool expanding;
	const char* name;

	/*Pointer hash set of macro_t*. macro is hidden from these toks*/
	hash_table_t* hidden_toks;

}macro_t;

typedef struct macro_expansion
{
	macro_t* macro;
	hash_table_t* params;

	//set when we are expanding a param
	//_emit_token() will add to this range if set
	token_range_t* param_expansion;

	//hash_table_t* expanded_macros;

	struct macro_expansion* next;
}macro_expansion_t;

typedef struct
{
	/*
	Map of identifier string to macro_t*
	*/
	hash_table_t* defs;

	/* result list*/
	token_range_t result;

	//hash_table_t* expanded_macros;

	macro_expansion_t* expansion_stack;

}pp_context_t;

bool pre_proc_eval_expr(pp_context_t* pp, token_range_t range, uint32_t* val);