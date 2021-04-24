#pragma once

//identifier map - maintains the set of known identifiers during semantic analysis

#include "ast.h"
#include "int_val.h"

#include <libj/include/hash_table.h>

/*
declaration kind
*/
typedef enum
{
	/*
	declaration - function, variable, type, alias
	*/
	id_decl,

	/*
	internal marker to seperate code blocks
	*/
	id_marker
}id_kind;

/*
an identifier stack item - either a named declaration
*/
typedef struct identifier
{
	id_kind kind;
	union
	{
		ast_declaration_t* decl;
	}data;
	struct identifier* next;
}identifier_t;


/*
'tag' or user type specification stack item
*/
typedef struct type
{
	/*
	type specification, set to null to represent end of code block in the stack
	*/
	ast_type_spec_t* spec;
	struct type* next;
}type_t;

/*
identifier map data structure,
*/
typedef struct
{
	/*
	stack of declarations
	*/
	identifier_t* identifiers;

	/*
	stack of user defined types (structs, unions & enums)
	*/
	type_t* tags;
	
	/*
	map of unique string literal value to label name 
	multiple string literals with the same value will map to the same label
	*/
	hash_table_t* string_literals;
}identfier_map_t;

/*
create an identifier map
*/
identfier_map_t* idm_create();

/*
destroy identifier map
*/
void idm_destroy(identfier_map_t*);

/*
add a declaration into the map, does not check for duplicates
*/
void idm_add_decl(identfier_map_t* map, ast_declaration_t* decl);

/*
add a tag into the map, does not check for duplicates
*/
void idm_add_tag(identfier_map_t* map, ast_type_spec_t* typeref);

/*
update the declaration, used when we encounter a definition after encountering the declaration
*/
ast_declaration_t* idm_update_decl(identfier_map_t* map, ast_declaration_t* decl);

/*
search the map for a declaration 
*/
ast_declaration_t* idm_find_decl(identfier_map_t* map, const char* name);

/*
search the map for a declaration in the current code block
*/
ast_declaration_t* idm_find_block_decl(identfier_map_t* map, const char* name);

/*
search the map for a tag
*/
ast_type_spec_t* idm_find_tag(identfier_map_t* map, const char* name);

/*
search the map for a tag in the current code block
*/
ast_type_spec_t* idm_find_block_tag(identfier_map_t* map, const char* name);

/*
call when entering a function to register declarations of the function parameters
*/
void idm_enter_function(identfier_map_t* map, ast_func_params_t* fn);

/*
call when leaving a function to unroll the stack
*/
void idm_leave_function(identfier_map_t* map);

/*
call when entering a code block to setup the stack
*/
void idm_enter_block(identfier_map_t* map);

/*
call when leaving a code block to unroll the stack
*/
void idm_leave_block(identfier_map_t* map);

/*
add a string literal to the map and return the allocated label
*/
const char* idm_add_string_literal(identfier_map_t* map, const char* literal);
