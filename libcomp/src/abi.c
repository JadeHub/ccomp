#include "abi.h"

#include <assert.h>

static size_t _calc_union_layout(ast_type_spec_t* spec)
{
	ast_struct_member_t* member = spec->data.user_type_spec->data.struct_members;
	size_t size = 0;

	while (member)
	{
		if (member->decl->type_ref->spec->size > size)
			size = member->decl->type_ref->spec->size;
		member = member->next;
	}

	//calculate tail padding
	size_t align = abi_get_type_alignment(spec);
	assert(align);
	//calculate the padding required to correctly align the member
	size_t pad = (align - (size % align)) % align;

	return size + pad;
}

static size_t _member_alloc_size(ast_struct_member_t* member)
{
	if (ast_is_array_decl(member->decl))
		return member->decl->sema.alloc_size;
	return member->decl->type_ref->spec->size;
}

size_t abi_calc_user_type_layout(ast_type_spec_t* spec)
{
	if (ast_type_is_enum(spec))
		return 4;

	if (spec->data.user_type_spec->kind == user_type_union)
		return _calc_union_layout(spec);

	ast_struct_member_t* member = spec->data.user_type_spec->data.struct_members;
	size_t offset = 0;

	while (member)
	{
		if (ast_is_bit_field_member(member))
		{
			/*
			This does not match the abi used by clang and gcc
			There is some minimal info and examples here https://refspecs.linuxfoundation.org/elf/abi386-4.pdf
			but I have not managed to replicate the required algorithm

			One odd case to investigate:
			This packs the s and i together into 3 bytes
			struct { short s : 9; int i : 9; char c; } ;

			Adding the initial char causes clang to add padding between s
			struct { char p; short s : 9; int i : 9; char c; } ; packs the s and i together into 3 bytes

			This command can be used to see how clang organises structs
			clang -cc1 -triple i386-pc-linux-gnu -fdump-record-layouts test.c
			*/

			if (member->sema.bit_field.size == 0)
			{
				//anonymous zero length field, stop packing this field
				member = member->next;
				continue;
			}

			//the field type dictates the alignment and the max number of bits
			size_t align = member->decl->type_ref->spec->size;
			assert(align);
			size_t pad = (align - (offset % align)) % align;
			offset += pad;

			size_t bit_sz = 0;
			do
			{
				//the member starts at offset
				member->sema.offset = offset;

				if (member->sema.bit_field.size == 0)
				{
					//anonymous zero length field, stop packing this field
					member = member->next;
					break;
				}

				//the field is offset by bit_sz bits
				member->sema.bit_field.offset = bit_sz;

				bit_sz += member->sema.bit_field.size;
				member = member->next;
			} while (member && ast_is_bit_field_member(member));

			offset += (bit_sz / 8) + (bit_sz % 8 ? 1 : 0); //align; //align is also the size of the field
		}
		else
		{
			size_t align = abi_get_type_alignment(member->decl->type_ref->spec);
			assert(align);
			//calculate the padding required to correctly align the member
			size_t pad = (align - (offset % align)) % align;
			member->sema.offset = offset + pad;

			offset = member->sema.offset + _member_alloc_size(member);
			member = member->next;
		}
	}

	//calculate tail padding
	size_t align = abi_get_type_alignment(spec);
	assert(align);
	//calculate the padding required to correctly align the member
	size_t pad = (align - (offset % align)) % align;

	return offset + pad;
}

size_t abi_get_type_alignment(ast_type_spec_t* spec)
{
	assert(spec->kind != type_alias); //assume the type has been 'resolved'

	if (spec->kind != type_user || ast_type_is_enum(spec))
		return spec->size;

	//take the max of the struct / union members alignment
	ast_struct_member_t* member = spec->data.user_type_spec->data.struct_members;
	size_t max_align = 0;
	while (member)
	{
		size_t align = abi_get_type_alignment(member->decl->type_ref->spec);

		if (align > max_align)
			max_align = align;
		member = member->next;
	}

	return max_align;
}