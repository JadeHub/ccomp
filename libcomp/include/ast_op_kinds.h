#pragma once

typedef enum
{
	op_unknown,
	//unary operators
	op_negate,
	op_compliment,
	op_not,
	op_address_of,
	op_dereference,
	op_prefix_inc,
	op_prefix_dec,
	op_postfix_inc,
	op_postfix_dec,
	//binary operators
	op_add,
	op_sub,
	op_mul,
	op_div,
	op_mod,

	op_shiftleft,
	op_shiftright,

	op_bitwise_or,
	op_bitwise_xor,
	op_bitwise_and,

	op_member_access,
	op_ptr_member_access,
	op_array_subscript,

	//logical operators
	op_and,
	op_or,
	op_eq,
	op_neq,
	op_lessthan,
	op_lessthanequal,
	op_greaterthan,
	op_greaterthanequal

}op_kind;
