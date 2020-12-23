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

	//assignment
	op_assign,
	op_mul_assign,
	op_div_assign,
	op_mod_assign,
	op_add_assign,
	op_sub_assign,
	op_left_shift_assign,
	op_right_shift_assign,
	op_and_assign,
	op_xor_assign,
	op_or_assign,

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
