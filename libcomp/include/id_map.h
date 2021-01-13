#pragma once

#include "ast.h"
#include "int_val.h"

#include <libj/include/hash_table.h>

/*
Types (struct, unions)
enums
functions
local variables
glocal variables
*/

typedef enum
{
	id_decl,
	id_const,
	id_marker
}id_kind;

typedef struct
{
	const char* name;
	int_val_t val;
}enum_val_t;

typedef struct identifier
{
	id_kind kind;
	union
	{
		ast_declaration_t* decl;
		enum_val_t enum_val;
	}data;
	struct identifier* next;
}identifier_t;

typedef struct type
{
	ast_type_spec_t* spec;
	struct type* next;
}type_t;

typedef struct string_literal
{
	const char* label;
	struct string_literal* next;
}string_literal_t;

typedef struct {

	identifier_t* identifiers;
	type_t* tags;
	
	//map literal value to label
	hash_table_t* string_literals;
}identfier_map_t;

identfier_map_t* idm_create();
void idm_destroy(identfier_map_t*);

void idm_add_id(identfier_map_t* map, ast_declaration_t* decl);
void idm_add_tag(identfier_map_t* map, ast_type_spec_t* typeref);
void idm_add_enum_val(identfier_map_t* map, const char* name, int_val_t val);

ast_declaration_t* idm_update_decl(identfier_map_t* map, ast_declaration_t* decl);
ast_declaration_t* idm_find_decl(identfier_map_t* map, const char* name);
ast_declaration_t* idm_find_block_decl(identfier_map_t* map, const char* name);
int_val_t* idm_find_enum_val(identfier_map_t* map, const char* name);
ast_type_spec_t* idm_find_tag(identfier_map_t* map, const char* name);
ast_type_spec_t* idm_find_block_tag(identfier_map_t* map, const char* name);

void idm_enter_function(identfier_map_t* map, ast_func_params_t* fn);
void idm_leave_function(identfier_map_t* map);

void idm_enter_block(identfier_map_t* map);
void idm_leave_block(identfier_map_t* map);

const char* idm_add_string_literal(identfier_map_t* map, const char* literal);