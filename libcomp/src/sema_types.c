#include "sema_internal.h"

#include "std_types.h"

#include <limits.h>

ast_type_spec_t* int_val_smallest_size(int_val_t* val)
{
	if (val->is_signed && val->int64 < 0)
	{
		if (int_val_will_fit(val, int8_type_spec))
			return int8_type_spec;
		if (int_val_will_fit(val, int16_type_spec))
			return int16_type_spec;
		if (int_val_will_fit(val, int32_type_spec))
			return int32_type_spec;
		if (int_val_will_fit(val, int64_type_spec))
			return int64_type_spec;
		return NULL;
	}
	if (int_val_will_fit(val, uint8_type_spec))
		return uint8_type_spec;
	if (int_val_will_fit(val, uint16_type_spec))
		return uint16_type_spec;
	if (int_val_will_fit(val, uint32_type_spec))
		return uint32_type_spec;
	if (int_val_will_fit(val, uint64_type_spec))
		return uint64_type_spec;
	return NULL;
}

uint64_t ast_int_type_max(ast_type_spec_t* type)
{
	switch (type->kind)
	{
	case type_int8:
		return SCHAR_MAX;
	case type_uint8:
		return UCHAR_MAX;
	case type_int16:
		return SHRT_MAX;
	case type_uint16:
		return USHRT_MAX;
	case type_int32:
		return LONG_MAX;
	case type_uint32:
		return ULONG_MAX;
	case type_int64:
		return LLONG_MAX;
	case type_uint64:
		return ULLONG_MAX;
	}
	return 0;
}

int64_t ast_int_type_min(ast_type_spec_t* type)
{
	switch (type->kind)
	{
	case type_int8:
		return SCHAR_MIN;
	case type_int16:
		return SHRT_MIN;
	case type_int32:
		return LONG_MIN;
	case type_int64:
		return LLONG_MIN;
	case type_uint8:
	case type_uint16:
	case type_uint32:
	case type_uint64:
		return 0;
	}
	return 0;
}

bool int_val_will_fit(int_val_t* val, ast_type_spec_t* type)
{
	if (!ast_type_is_int(type))
		return false;

	if (val->is_signed)
	{
		return val->int64 >= ast_int_type_min(type) && val->int64 <= (int64_t)ast_int_type_max(type);
	}
	return val->uint64 <= ast_int_type_max(type);

	
}
