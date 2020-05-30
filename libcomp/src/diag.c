#include "diag.h"

#include <stddef.h>
#include <assert.h>

static diag_cb _diag_cb = NULL;

void diag_set_handler(diag_cb cb)
{
	_diag_cb = cb;
}

void diag_err(struct token* tok, uint32_t err, const char* format, ...)
{
	assert(_diag_cb);
	char buff[512];

	va_list args;
	va_start(args, format);
	vsnprintf(buff, 512, format, args);
	va_end(args);

	_diag_cb(tok, err, buff);
}