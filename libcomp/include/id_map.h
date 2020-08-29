#pragma once

#include "ast.h"

/*
Types (struct, unions)
enums
functions
local variables
glocal variables
*/

typedef struct identifier
{
	ast_declaration_t* decl;
	struct identifier* next;
}identifier_t;

typedef struct type
{
	ast_type_spec_t* spec;
	struct type* next;
}type_t;

typedef struct {

	identifier_t* identifiers;
	type_t* tags;
}identfier_map_t;

identfier_map_t* idm_create();
void idm_destroy(identfier_map_t*);

void idm_add_id(identfier_map_t* map, ast_declaration_t* decl);
void idm_add_tag(identfier_map_t* map, ast_type_spec_t* typeref);

ast_declaration_t* idm_update_decl(identfier_map_t* map, ast_declaration_t* decl);
ast_declaration_t* idm_find_decl(identfier_map_t* map, const char* name, ast_decl_type kind);
ast_declaration_t* idm_find_block_decl(identfier_map_t* map, const char* name, ast_decl_type kind);

ast_type_spec_t* idm_find_tag(identfier_map_t* map, const char* name);
ast_type_spec_t* idm_find_block_tag(identfier_map_t* map, const char* name);

void idm_enter_function(identfier_map_t* map, ast_function_decl_t* fn);
void idm_leave_function(identfier_map_t* map);

void idm_enter_block(identfier_map_t* map);
void idm_leave_block(identfier_map_t* map);