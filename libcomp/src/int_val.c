#include "int_val.h"

#include <limits.h>
#include <string.h>

static inline bool _is_zero(int_val_t* v)
{
	return v->uint64 == 0;
}

static int_val_t _addition(int_val_t* lhs, int_val_t* rhs)
{
	if (lhs->is_signed)
		lhs->int64 += rhs->is_signed ? rhs->int64 : rhs->uint64;
	else
		lhs->uint64 += rhs->is_signed ? rhs->int64 : rhs->uint64;
	return *lhs;
}

static int_val_t _subtraction(int_val_t* lhs, int_val_t* rhs)
{
	if (lhs->is_signed)
	{
		lhs->int64 -= rhs->is_signed ? rhs->int64 : rhs->uint64;
	}
	else
	{
		//does it end below 0
		if ((rhs->is_signed ? rhs->int64 : rhs->uint64) > lhs->uint64)
		{
			//result becomes signed
			lhs->int64 = lhs->uint64 - (rhs->is_signed ? rhs->int64 : rhs->uint64);
			lhs->is_signed = true;
		}
		else
		{
			lhs->uint64 -= rhs->is_signed ? rhs->int64 : rhs->uint64;
		}
	}
	return *lhs;
}

static int_val_t _multiplication(int_val_t* lhs, int_val_t* rhs)
{
	if (lhs->is_signed)
	{
		//result remains signed
		lhs->int64 *= rhs->is_signed ? rhs->int64 : rhs->uint64;
	}
	else if (rhs->is_signed)
	{
		//result becomes signed
		lhs->int64 = lhs->uint64 * rhs->int64;
		lhs->is_signed = true;
	}
	else
	{
		//result remains unsigned
		lhs->uint64 *= rhs->uint64;
	}
	return *lhs;
}

static int_val_t _division(int_val_t* lhs, int_val_t* rhs)
{
	if (lhs->is_signed)
	{
		lhs->int64 = lhs->int64 / rhs->int64; //todo ignoring rhs->is_signed = true
	}
	else
	{
		if (rhs->is_signed)
		{
			lhs->int64 = lhs->int64 / rhs->int64;
			lhs->is_signed = true;
		}
		else
		{
			lhs->uint64 /= rhs->uint64;
		}
	}
	return *lhs;
}

static int_val_t _modulus(int_val_t* lhs, int_val_t* rhs)
{
	if (lhs->is_signed)
	{
		lhs->uint64 = lhs->int64 % rhs->is_signed ? rhs->int64 : rhs->uint64;
		lhs->is_signed = false;
	}
	else
	{
		lhs->uint64 %= rhs->is_signed ? rhs->int64 : rhs->uint64;
	}
	return *lhs;
}

static int_val_t _shiftleft(int_val_t* lhs, int_val_t* rhs)
{
	if (lhs->is_signed)
	{
		lhs->int64 = lhs->int64 << (rhs->is_signed ? rhs->int64 : rhs->uint64);
	}
	else
	{
		lhs->uint64 = lhs->uint64 << (rhs->is_signed ? rhs->int64 : rhs->uint64);
	}
	return *lhs;
}

static int_val_t _shiftright(int_val_t* lhs, int_val_t* rhs)
{
	if (lhs->is_signed)
	{
		lhs->int64 = lhs->int64 >> (rhs->is_signed ? rhs->int64 : rhs->uint64);
	}
	else
	{
		lhs->uint64 = lhs->uint64 >> (rhs->is_signed ? rhs->int64 : rhs->uint64);
	}
	return *lhs;
}

static bool _lessthan(int_val_t* lhs, int_val_t* rhs)
{
	if (lhs->is_signed)
		return lhs->int64 < (rhs->is_signed ? rhs->int64 : rhs->uint64);
	return lhs->uint64 < (rhs->is_signed ? rhs->int64 : rhs->uint64);
}

static bool _lessthanequal(int_val_t* lhs, int_val_t* rhs)
{
	if (lhs->is_signed)
		return lhs->int64 <= (rhs->is_signed ? rhs->int64 : rhs->uint64);
	return lhs->uint64 <= (rhs->is_signed ? rhs->int64 : rhs->uint64);
}

static bool _greaterthan(int_val_t* lhs, int_val_t* rhs)
{
	if (lhs->is_signed)
		return lhs->int64 > (rhs->is_signed ? rhs->int64 : rhs->uint64);
	return lhs->uint64 > (rhs->is_signed ? rhs->int64 : rhs->uint64);
}

static bool _greaterthanequal(int_val_t* lhs, int_val_t* rhs)
{
	if (lhs->is_signed)
		return lhs->int64 >= (rhs->is_signed ? rhs->int64 : rhs->uint64);
	return lhs->uint64 >= (rhs->is_signed ? rhs->int64 : rhs->uint64);
}

static int_val_t _eq(int_val_t* lhs, int_val_t* rhs)
{
	bool b = lhs->uint64 == rhs->uint64;	
	return b ? int_val_one() : int_val_zero();
}

int_val_t int_val_zero()
{
	return int_val_unsigned(0);
}

int_val_t int_val_one()
{
	return int_val_unsigned(1);
}

int_val_t int_val_unsigned(uint64_t val)
{
	int_val_t result;
	result.uint64 = val;
	result.is_signed = false;
	return result;
}

int_val_t int_val_signed(int64_t val)
{
	int_val_t result;
	result.int64 = val;
	result.is_signed = true;
	return result;
}

int_val_t int_val_binary_op(int_val_t* lhs, int_val_t* rhs, op_kind op)
{
	int_val_t result = *lhs;

	switch (op)
	{
	case op_add:
		result = _addition(lhs, rhs);
		break;
	case op_mul:
		result = _multiplication(lhs, rhs);
		break;
	case op_sub:
		result = _subtraction(lhs, rhs);
		break;
	case op_div:
		result = _division(lhs, rhs);
		break;
	case op_mod:
		result = _modulus(lhs, rhs);
		break;
	case op_shiftleft:
		result = _shiftleft(lhs, rhs);
		break;
	case op_shiftright:
		result = _shiftright(lhs, rhs);
		break;
	case op_eq:
		result = int_val_unsigned(lhs->uint64 == rhs->uint64);
		break;
	case op_neq:
		result = int_val_unsigned(lhs->uint64 != rhs->uint64);
		break;
	case op_and:
		result = int_val_unsigned(lhs->uint64 && rhs->uint64);
		break;
	case op_or:
		result = int_val_unsigned(lhs->uint64 || rhs->uint64);
		break;
	case op_lessthan:
		result = int_val_unsigned(_lessthan(lhs, rhs));
		break;
	case op_lessthanequal:
		result = int_val_unsigned(_lessthanequal(lhs, rhs));
		break;
	case op_greaterthan:
		result = int_val_unsigned(_greaterthan(lhs, rhs));
		break;
	case op_greaterthanequal:
		result = int_val_unsigned(_greaterthanequal(lhs, rhs));
		break;
	case op_bitwise_and:
		result = int_val_unsigned(lhs->uint64 & rhs->uint64);
		break;
	case op_bitwise_or:
		result = int_val_unsigned(lhs->uint64 | rhs->uint64);
		break;
	case op_bitwise_xor:
		result = int_val_unsigned(lhs->uint64 ^ rhs->uint64);
		break;
	}	
	
	if (result.int64 == 0)
		result.is_signed = false;

	return result;
}

int_val_t int_val_unary_op(int_val_t* val, op_kind op)
{
	int_val_t result = int_val_zero();

	switch (op)
	{
	case op_negate:
		if (_is_zero(val))
			break;
		else if (val->is_signed)
		{
			result.int64 = -val->int64;
			result.is_signed = false;
		}
		else
		{
			result.int64 = val->uint64;
			result.int64 = -result.int64;
			result.is_signed = true;
		}
		break;
	case op_compliment:
		if (val->is_signed)
		{
			result.int64 = ~val->int64;
			result.is_signed = true;
		}
		else
		{
			result.uint64 = ~val->uint64;
			result.is_signed = false;
		}
		break;
	case op_not:
		if (val->is_signed)
		{
			result.int64 = !val->int64;
			result.is_signed = true;
		}
		else
		{
			result.uint64 = !val->uint64;
			result.is_signed = false;
		}
		break;
	}
	if (result.int64 == 0)
		result.is_signed = false;
	return result;
}

bool int_val_eq(int_val_t* lhs, int_val_t* rhs)
{
	if (lhs->is_signed != rhs->is_signed)
		return false;

	return lhs->is_signed ?
		lhs->int64 == rhs->int64 :
		lhs->uint64 == rhs->uint64;
}

uint32_t int_val_as_uint32(int_val_t* val)
{
	return val->is_signed ? (uint32_t)val->int64 : (uint32_t)val->uint64;
}

int32_t int_val_as_int32(int_val_t* val)
{
	return val->is_signed ? (int32_t)val->int64 : (int32_t)val->uint64;
}

int_val_t int_val_inc(int_val_t val)
{
	int_val_t one = int_val_one();
	return int_val_binary_op(&val, &one, op_add);
}

size_t int_val_required_width(int_val_t* val)
{
	if (val->is_signed)
	{
		if (val->int64 >= SCHAR_MIN && val->int64 <= SCHAR_MAX)
			return 8;
		if (val->int64 >= SHRT_MIN && val->int64 <= SHRT_MAX)
			return 16;
		if (val->int64 >= LONG_MIN && val->int64 <= LONG_MAX)
			return 32;
	}
	else
	{
		if (val->uint64 <= UCHAR_MAX)
			return 8;
		else if (val->uint64 <= USHRT_MAX)
			return 16;
		else if (val->uint64 <= ULONG_MAX)
			return 32;
	}
	return 64;
}