#include "diag.h"
#include "source.h"
#include "token.h"

#include <stddef.h>
#include <assert.h>
#include <stdio.h>

static diag_cb _diag_cb = NULL;
static void* _cb_data = NULL;

void diag_set_handler(diag_cb cb, void* data)
{
	_diag_cb = cb;
	_cb_data = data;
}

void diag_err(token_t* tok, uint32_t err, const char* format, ...)
{
	assert(_diag_cb);
	char buff[512];

	va_list args;
	va_start(args, format);
	vsnprintf(buff, 512, format, args);
	va_end(args);

	_diag_cb(tok, err, buff, _cb_data);
}

const char* diag_pos_str(token_t* tok)
{
	return src_file_pos_str(src_file_position(tok->loc));
}