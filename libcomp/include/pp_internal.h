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
	token_t* define;	//#define which introduced this macro
	macro_kind kind;
	token_range_t tokens;
	token_t* fn_params;
	const char* name;

	/*Pointer hash set of macro_t*. macro is hidden from these toks*/
	hash_table_t* hidden_toks;
}macro_t;

typedef struct expansion_context
{
	macro_t* macro;
	hash_table_t* params;

	bool complete;

	struct expansion_context* next;
}expansion_context_t;

typedef struct input_range
{
	token_range_t* tokens;
	token_t* current;

	expansion_context_t* macro_expansion;

	const char* path;
	uint32_t line_num;

	struct input_range* next;
}input_range_t;

typedef struct dest_range
{
	token_range_t* range;
	uint8_t next_tok_flags;
	struct dest_range* next;
}dest_range_t;

#define FLAGS_UNSET 0xFF

typedef struct
{
	/*
	Map of identifier string to macro_t*
	*/
	hash_table_t* defs;

	//token_range_t input;

	/* result list*/
	token_range_t result;

	expansion_context_t* expansion_stack;

	dest_range_t* dest_stack;

	input_range_t* input_stack;

	uint8_t define_id_supression_state;

	hash_table_t* praga_once_paths;
}pp_context_t;

bool pre_proc_eval_expr(pp_context_t* pp, token_range_t range, uint32_t* val);
