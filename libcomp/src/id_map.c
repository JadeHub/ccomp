#include "id_map.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static inline bool _id_is_decl(identifier_t* id, const char* name)
{
	return (id->kind == id_decl &&
		strcmp(name, ast_declaration_name(id->data.decl)) == 0);
}

identfier_map_t* idm_create()
{
	identfier_map_t* result = (identfier_map_t*)malloc(sizeof(identfier_map_t));
	memset(result, 0, sizeof(identfier_map_t));
	result->string_literals = sht_create(32);
	return result;
}

void idm_destroy(identfier_map_t* map)
{	
	free(map);
}

static const char* _alloc_label()
{
	static uint32_t count = 0;

	char* ret = (char*)malloc(16);
	memset(ret, 0, 16);
	sprintf(ret, "SC%d", count++);
	return ret;
}

const char* idm_add_string_literal(identfier_map_t* map, const char* literal)
{
	string_literal_t* sl = (string_literal_t*)sht_lookup(map->string_literals, literal);
	if (!sl)
	{
		sl = (string_literal_t*)malloc(sizeof(string_literal_t));
		sl->label = _alloc_label();
		sht_insert(map->string_literals, literal, sl);
	}
	return sl->label;
}

void idm_add_enum_val(identfier_map_t* map, const char* name, int_val_t val)
{
	identifier_t* id = (identifier_t*)malloc(sizeof(identifier_t));
	memset(id, 0, sizeof(identifier_t));

	id->kind = id_const;
	id->data.enum_val.name = strdup(name);
	id->data.enum_val.val = val;
	id->next = map->identifiers;
	map->identifiers = id;
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

	id->kind = id_decl;
	id->data.decl = decl;
	id->next = map->identifiers;
	map->identifiers = id;
}

int_val_t* idm_find_enum_val(identfier_map_t* map, const char* name)
{
	identifier_t* id = map->identifiers;

	while (id)
	{
		if (id->kind == id_const &&
			strcmp(name, id->data.enum_val.name) == 0)
		{
			return &id->data.enum_val.val;
		}
		id = id->next;
	}
	return NULL;
}

ast_type_spec_t* idm_find_block_tag(identfier_map_t* map, const char* name)
{
	type_t* tag = map->tags;

	while (tag)
	{
		if (!tag->spec) //indicates the end of the current scope block
			break;

		if (strcmp(name, tag->spec->data.user_type_spec->name) == 0)
		{
			return tag->spec;
		}
		tag = tag->next;
	}
	return NULL;
}

ast_type_spec_t* idm_find_tag(identfier_map_t* map, const char* name)
{
	type_t* tag = map->tags;

	while (tag)
	{
		if (tag->spec &&
			strcmp(name, tag->spec->data.user_type_spec->name) == 0)
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
		if(_id_is_decl(id, ast_declaration_name(decl)) && id->kind == decl->kind)
		{
			id->data.decl = decl;
			return id->data.decl;
		}
		id = id->next;
	}
	return NULL;
}

ast_declaration_t* idm_find_decl(identfier_map_t* map, const char* name)
{
	identifier_t* id = map->identifiers;

	while (id)
	{
		if (_id_is_decl(id, name))
		{
			return id->data.decl;
		}
		id = id->next;
	}
	return NULL;
}

ast_declaration_t* idm_find_block_decl(identfier_map_t* map, const char* name)
{
	identifier_t* id = map->identifiers;

	while (id)
	{
		if (id->kind == id_marker)
			break;

		if (_id_is_decl(id, name))
		{
			return id->data.decl;
		}
		id = id->next;
	}
	return NULL;
}


void idm_enter_function(identfier_map_t* map, ast_func_params_t* params)
{
	idm_enter_block(map);
	

	ast_func_param_decl_t* param = params->first_param;
	while (param)
	{
		idm_add_id(map, param->decl);
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

	id->kind = id_marker;
	id->next = map->identifiers;
	map->identifiers = id;

	//add block market to type stack
	type_t* type = (type_t*)malloc(sizeof(type_t));
	memset(type, 0, sizeof(type_t));

	type->spec = NULL;
	type->next = map->tags;
	map->tags = type;
}

void idm_leave_block(identfier_map_t* map)
{
	identifier_t* id = map->identifiers;	
	// remove all identifiers up to and including the most recent block marker
	while (id)
	{
		identifier_t* next = id->next;
		if (id->kind == id_marker)
		{
			map->identifiers = id->next;
			free(id);
			break;
		}
		free(id);
		id = next;
	}

	type_t* type = map->tags;
	// remove all tags up to and including the most recent block marker
	while (type)
	{
		type_t* next = type->next;
		if (type->spec == NULL)
		{
			map->tags = type->next;
			free(type);
			break;
		}
		free(type);
		type = next;
	}
}
