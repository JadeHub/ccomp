#include "decl_map.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>


identfier_map_t* idm_create()
{
	identfier_map_t* result = (identfier_map_t*)malloc(sizeof(identfier_map_t));
	memset(result, 0, sizeof(identfier_map_t));
	return result;
}

void idm_destroy(identfier_map_t* map)
{
	
	free(map);
}

void idm_add_tag(identfier_map_t* map, ast_type_spec_t* type)
{
	type_t* tag = (type_t*)malloc(sizeof(type_t));
	memset(tag, 0, sizeof(type_t));

	tag->spec = type;
	tag->next = map->tags;
	map->tags = tag;
}

void idm_add_id(identfier_map_t* map, ast_declaration_t* decl)
{
	identifier_t* id = (identifier_t*)malloc(sizeof(identifier_t));
	memset(id, 0, sizeof(identifier_t));

	id->decl = decl;
	id->next = map->identifiers;
	map->identifiers = id;
}

ast_type_spec_t* idm_find_tag(identfier_map_t* map, const char* name)
{
	type_t* tag = map->tags;

	while (tag)
	{
		if (strcmp(name, tag->spec->struct_spec->name) == 0)
		{
			return tag->spec;
		}
		tag = tag->next;
	}
	return NULL;
}

ast_declaration_t* idm_update_decl(identfier_map_t* map, ast_declaration_t* decl)
{
	identifier_t* id = map->identifiers;

	while (id)
	{
		if (id->decl &&
			id->decl->kind == decl->kind &&
			strcmp(ast_declaration_name(decl), ast_declaration_name(id->decl)) == 0)
		{
			id->decl = decl;
			return id->decl;
		}
		id = id->next;
	}
	return NULL;
}

ast_declaration_t* idm_find_decl(identfier_map_t* map, const char* name, ast_decl_type kind)
{
	identifier_t* id = map->identifiers;

	while (id)
	{
		if (id->decl && 
			id->decl->kind == kind && 
			strcmp(name, ast_declaration_name(id->decl)) == 0)
		{
			return id->decl;
		}
		id = id->next;
	}
	return NULL;
}

ast_declaration_t* idm_find_block_decl(identfier_map_t* map, const char* name, ast_decl_type kind)
{
	identifier_t* id = map->identifiers;

	while (id)
	{
		if (!id->decl)
			break;

		if (id->decl->kind == kind &&
			strcmp(name, ast_declaration_name(id->decl)) == 0)
		{
			return id->decl;
		}
		id = id->next;
	}
	return NULL;
}

void idm_enter_function(identfier_map_t* map, ast_function_decl_t* fn)
{
	idm_enter_block(map);

	ast_declaration_t* param = fn->params;
	while (param)
	{
		idm_add_id(map, param);
		param = param->next;
	}
}

void idm_leave_function(identfier_map_t* map)
{
	idm_leave_block(map);
}

void idm_enter_block(identfier_map_t* map)
{
	//add block marker to identifier stack
	identifier_t* id = (identifier_t*)malloc(sizeof(identifier_t));
	memset(id, 0, sizeof(identifier_t));

	id->decl = NULL;
	id->next = map->identifiers;
	map->identifiers = id;

	//add block market to type stacl
}

void idm_leave_block(identfier_map_t* map)
{
	identifier_t* id = map->identifiers;	

	// remove all up to and including the most recent block marker
	while (id)
	{
		identifier_t* next = id->next;
		if (id->decl == NULL)
		{
			map->identifiers = id->next;
			free(id);
			return;
		}
		free(id);
		id = next;
	}
	assert(0);
}
