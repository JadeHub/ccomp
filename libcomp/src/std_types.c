#include "std_types.h"

/*built in types */
/* {{token_range_t}, kind, size, flags, user_type_spec} */
static ast_type_spec_t _void_type = { {NULL, NULL}, type_void,		0, NULL };
static ast_type_spec_t _char_type = { {NULL, NULL}, type_int8,		1, NULL };
static ast_type_spec_t _uchar_type = { {NULL, NULL}, type_uint8,	1, NULL };
static ast_type_spec_t _short_type = { {NULL, NULL}, type_int16,	2, NULL };
static ast_type_spec_t _ushort_type = { {NULL, NULL}, type_uint16,	2, NULL };
static ast_type_spec_t _int_type = { {NULL, NULL}, type_int32,		4, NULL };
static ast_type_spec_t _uint_type = { {NULL, NULL}, type_uint32,	4, NULL };


ast_type_spec_t* void_type_spec = NULL;
ast_type_spec_t* int8_type_spec = NULL;
ast_type_spec_t* uint8_type_spec = NULL;
ast_type_spec_t* int16_type_spec = NULL;
ast_type_spec_t* uint16_type_spec = NULL;
ast_type_spec_t* int32_type_spec = NULL;
ast_type_spec_t* uint32_type_spec = NULL;

void types_init()
{
	void_type_spec = &_void_type;
	int8_type_spec = &_char_type;
	uint8_type_spec = &_uchar_type;
	int16_type_spec = &_short_type;
	uint16_type_spec = &_ushort_type;
	int32_type_spec = &_int_type;
	uint32_type_spec = &_uint_type;
}