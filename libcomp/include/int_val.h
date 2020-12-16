#pragma once

#include "ast_op_kinds.h"
#include "ast_type_spec.h"

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#define IV_SIGNED		1
#define IV_OVERFLOW		2
#define IV_ERR		2

typedef struct
{
	union
	{
		int64_t	int64;
		uint64_t uint64;
	}v;

	bool is_signed;
}int_val_t;

int_val_t int_val_zero();
int_val_t int_val_one();

int_val_t int_val_unsigned(uint64_t val);
int_val_t int_val_signed(int64_t val);

uint32_t int_val_as_uint32(int_val_t* val);
int32_t int_val_as_int32(int_val_t* val);

//todo - only used to generate enum values which needs reworking
int_val_t int_val_inc(int_val_t val);

int_val_t int_val_unary_op(int_val_t* val, op_kind op);
int_val_t int_val_binary_op(int_val_t* lhs, int_val_t* rhs, op_kind op);

bool int_val_eq(int_val_t* lhs, int_val_t* rhs);

size_t int_val_required_width(int_val_t* val);