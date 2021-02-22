#include "abi.h"

#include <assert.h>

/*size_t abi_calc_var_decl_stack_size(ast_declaration_t* decl)
{
	if (decl->array_sz)
	{
		assert(decl->array_sz->kind == expr_int_literal && decl->type_ref->spec->kind == type_ptr);
		return decl->array_sz->data.int_literal.val.v.int64 * decl->type_ref->spec->data.ptr_type->size;
	}
	return decl->type_ref->spec->size;
}*/