#pragma once

#include "ast.h"

extern ast_type_spec_t* void_type_spec;
extern ast_type_spec_t* int8_type_spec;
extern ast_type_spec_t* uint8_type_spec;
extern ast_type_spec_t* int16_type_spec;
extern ast_type_spec_t* uint16_type_spec;
extern ast_type_spec_t* int32_type_spec;
extern ast_type_spec_t* uint32_type_spec;
extern ast_type_spec_t* int64_type_spec;
extern ast_type_spec_t* uint64_type_spec;

void types_init();