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
	if (member->decl->array_dimensions)
		return member->decl->sema.alloc_size;
	return member->decl->type_ref->spec->size;
}

static size_t _count_adjacent_bit_fields(ast_struct_member_t* member)
{
	size_t result = 0;

	while (member && ast_is_bit_field_member(member))
	{
		result += member->sema.bit_size;
		member = member->next;
	}
	return result;
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
		/*if (ast_is_bit_field_member(member))
		{
			size_t sz = _count_adjacent_bit_fields(member);

			ast_struct_member_t* bit_field = member;
			size_t bits = 0;
			while(bit_field && ast_is_bit_field_member(bit_field))
			{
				bits += bit_field->sema.bit_size;
				bit_field = bit_field->next;
			}
			member = bit_field;
			continue;
		}*/

		size_t align = abi_get_type_alignment(member->decl->type_ref->spec);
		assert(align);
		//calculate the padding required to correctly align the member
		size_t pad = (align - (offset % align)) % align;
		member->sema.offset = offset + pad;

		offset = member->sema.offset + _member_alloc_size(member);
		member = member->next;
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