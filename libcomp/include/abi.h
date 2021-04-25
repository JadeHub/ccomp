#pragma once

#include "ast.h"

/*
Set the offset for each member and the alignment for the user type
*/
size_t abi_calc_user_type_layout(ast_type_spec_t* spec);

/*
Return the alignment requirement in bytes for the given type
*/
size_t abi_get_type_alignment(ast_type_spec_t* spec);