#include "nps.h"

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct ptr_item
{
	const char* name;
	void* ptr;
	struct ptr_item* next;
}ptr_item_t;

typedef struct ptr_set
{
	ptr_item_t* items;
}ptr_set_t;

ptr_set_t* ps_create()
{
	ptr_set_t* set = (ptr_set_t*)malloc(sizeof(ptr_set_t));
	memset(set, 0, sizeof(ptr_set_t));
	return set;
}

void ps_destroy(ptr_set_t* set)
{
	ptr_item_t* item = set->items;
	ptr_item_t* next;

	while (item)
	{
		next = item->next;
		free(item);
		item = next;
	}
	free(set);
}

void* ps_lookup(ptr_set_t* set, void* val)
{
	ptr_item_t* item = set->items;

	while (item)
	{
		if (item->ptr == val)
			return item->ptr;
		item = item->next;
	}
	return NULL;
}

void ps_insert(ptr_set_t* set, void* ptr)
{
	ptr_item_t* item = (ptr_item_t*)malloc(sizeof(ptr_item_t));
	memset(item, 0, sizeof(ptr_item_t));
	item->ptr = ptr;
	item->next = set->items;
	set->items = item;
}

bool ps_remove(ptr_set_t* set, void* val)
{
	if (!set->items) return false;

	ptr_item_t* item = set->items->next;
	ptr_item_t* prev = set->items;
	while (item)
	{
		if (item->ptr == val)
		{
			prev->next = item->next;
			free(item);
			return true;
		}
		prev = item;
		item = item->next;
	}
	return false;
}