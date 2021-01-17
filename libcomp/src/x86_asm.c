#include "x86_asm.h"

#include <assert.h>
#include <stdbool.h>
#include <stdarg.h>

static write_asm_cb _asm_cb;
static void* _asm_cb_data;

static void _gen_asm(const char* format, ...)
{
	char buff[256];

	va_list args;
	va_start(args, format);
	vsnprintf(buff, 256, format, args);
	va_end(args);

	_asm_cb(buff, _asm_cb_data);
}

static inline const char* _reg_name(x86_reg r)
{
	switch (r)
	{
	case REG_AL:
		return "%al";
	case REG_EAX:
		return "%eax";
	case REG_EBX:
		return "%ebx";
	case REG_ECX:
		return "%ecx";
	case REG_EDX:
		return "%edx";
	case REG_EBP:
		return "%ebp";
	case REG_ESP:
		return "%esp";
	}
	assert(false);
	return "";
}

static const char* _sized_mov(size_t width)
{
	switch (width)
	{
	case 4: return "movl";
	case 2: return "movw";
	case 1: return "movb";
	}

	assert(false);
	return "";
}

void x86_movl(x86_reg src, x86_reg dest)
{
	_gen_asm("movl %s, %s", _reg_name(src), _reg_name(dest));
}

void x86_movl_label(x86_reg src, const char* label, int32_t dest_offset)
{
	_gen_asm("movl %s, %s + %d", _reg_name(src), label, dest_offset);
}

void x86_movl_deref_source(x86_reg src, x86_reg dest, int32_t src_offset)
{
	if (src_offset)
		_gen_asm("movl %d(%s), %s", src_offset, _reg_name(src), _reg_name(dest));
	else
		_gen_asm("movl (%s), %s", _reg_name(src), _reg_name(dest));
}

void x86_movl_deref_dest(x86_reg src, x86_reg dest, int32_t dest_offset)
{
	if (dest_offset)
		_gen_asm("movl %s, %d(%s)", _reg_name(src), dest_offset, _reg_name(dest));
	else
		_gen_asm("movl %s, (%s)", _reg_name(src), _reg_name(dest));
}

void x86_movl_literal(int32_t val, x86_reg dest)
{
	_gen_asm("movl $%d, %s", val, _reg_name(dest));
}

void x86_movl_literal_deref_dest(int32_t val, x86_reg dest, int32_t dest_offset)
{
	_gen_asm("movl $%d, %d(%s)", val, dest_offset, _reg_name(dest));
}

void x86_mov_sz_deref_dest(size_t width, x86_reg src, x86_reg dest, int32_t dest_offset)
{
	if(dest_offset)
		_gen_asm("%s %s, %d(%s)", _sized_mov(width), _reg_name(src), dest_offset, _reg_name(dest));
	else
		_gen_asm("%s %s, (%s)", _sized_mov(width), _reg_name(src), _reg_name(dest));
}

void x86_mov_sz_label_dest(size_t width, x86_reg src, const char* dest_label, int32_t dest_offset)
{
	if (dest_offset)
		_gen_asm("%s %s, %s + %d", _sized_mov(width), _reg_name(src), dest_label, dest_offset);
	else
		_gen_asm("%s %s, %s", _sized_mov(width), _reg_name(src), dest_label);
}

void x86_movb_promote(bool unsigned_, x86_reg src, x86_reg dest)
{
	_gen_asm("%s %s, %s", unsigned_ ? "movzbl" : "movsbl", _reg_name(src), _reg_name(dest));

}

void x86_init(write_asm_cb cb, void* cb_data)
{
	_asm_cb = cb;
	_asm_cb_data = cb_data;
}
