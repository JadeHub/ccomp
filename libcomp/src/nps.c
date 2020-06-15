#include "nps.h"

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct named_ptr_item
{
	const char* name;
	void* ptr;
	struct named_ptr_item* next;
}named_ptr_item_t;

typedef struct named_ptr_set
{
	named_ptr_item_t* items;
}named_ptr_set_t;

named_ptr_set_t* nps_create()
{
	named_ptr_set_t* set = (named_ptr_set_t*)malloc(sizeof(named_ptr_set_t));
	memset(set, 0, sizeof(named_ptr_set_t));
	return set;
}

void nps_destroy(named_ptr_set_t* set)
{
	named_ptr_item_t* item = set->items;
	named_ptr_item_t* next;

	while (item)
	{
		next = item->next;
		free((void*)item->name);
		free(item);
		item = next;
	}
	free(set);
}

void* nps_lookup(named_ptr_set_t* set, const char* name)
{
	named_ptr_item_t* item = set->items;

	while (item)
	{
		if (strcmp(item->name, name) == 0)
			return item->ptr;
		item = item->next;
	}
	return NULL;
}

void nps_insert(named_ptr_set_t* set, const char* name, void* ptr)
{
	named_ptr_item_t* item = (named_ptr_item_t*)malloc(sizeof(named_ptr_item_t));
	memset(item, 0, sizeof(named_ptr_item_t));
	item->name = strdup(name);
	item->ptr = ptr;
	item->next = set->items;
	set->items = item;
}

bool nps_remove(named_ptr_set_t* set, const char* name)
{
	if (!set->items) return;

	if (strcmp(set->items->name, name) == 0)
	{
		set->items = set->items->next;
		return true;
	}

	named_ptr_item_t* item = set->items->next;
	named_ptr_item_t* prev = set->items;
	while (item)
	{
		if (strcmp(item->name, name) == 0)
		{
			prev->next = item->next;
			return true;
		}
		prev = item;
		item = item->next;
	}
	return false;
}