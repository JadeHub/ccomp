#pragma once

#include <stdint.h>

typedef struct
{
	uint32_t val;
}ast_const_t;

typedef struct
{
	//type

	ast_const_t val;

}ast_statement_t;

typedef struct
{
	char name[32];

	//params
	//return type
	//static?

	ast_statement_t* return_statement;

}ast_function_decl_t;

typedef struct
{
	ast_function_decl_t* function;
}ast_trans_unit_t;
