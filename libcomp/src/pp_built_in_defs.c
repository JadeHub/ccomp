#include "pp_internal.h"

const char* _built_in_defs = " \
#define COMP_JCC_1 1\n \
\n \
#define _jcc_va_list unsigned char*\n \
#define _jcc_va_start(list, param) (list = (((_jcc_va_list)&param) + sizeof(param)))\n \
#define _jcc_va_arg(list, type) (*(type *)((list += sizeof(type)) - sizeof(type)))\n \
#define _jcc_va_end(list) list\n \
";

const char* pp_built_in_defs()
{
	return _built_in_defs;
}