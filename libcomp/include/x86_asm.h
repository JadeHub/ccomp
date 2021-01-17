#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

typedef enum
{
	REG_AL,
	REG_EAX,
	REG_EBX,
	REG_ECX,
	REG_EDX,

	REG_ESP,
	REG_EBP
}x86_reg;

typedef void (*write_asm_cb)(const char* line, void* data);
void x86_init(write_asm_cb cb, void* cb_data);

void x86_movl(x86_reg src, x86_reg dest);
void x86_movl_label(x86_reg src, const char* label, int32_t dest_offset);
void x86_movl_deref_source(x86_reg src, x86_reg dest, int32_t src_offset);
void x86_movl_deref_dest(x86_reg src, x86_reg dest, int32_t dest_offset);
void x86_movl_literal(int32_t val, x86_reg dest);
void x86_movl_literal_deref_dest(int32_t val, x86_reg dest, int32_t dest_offset);

void x86_movb_promote(bool signed_, x86_reg src, x86_reg dest);

void x86_mov_sz_deref_dest(size_t width, x86_reg src, x86_reg dest, int32_t dest_offset);
void x86_mov_sz_label_dest(size_t width, x86_reg src, const char* dest_label, int32_t dest_offset);