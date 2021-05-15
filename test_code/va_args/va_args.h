#ifdef COMP_JCC_1

#define va_list _jcc_va_list
#define va_start _jcc_va_start
#define va_end _jcc_va_end
#define va_arg _jcc_va_arg

#else

#include <stdarg.h>

#endif

