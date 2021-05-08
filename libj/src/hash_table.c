#include "hash_table.h"

#include <stdio.h>
#include <string.h>

hash_table_t* ht_create(uint32_t sz, ht_hash_fn hash, ht_key_comp_fn key_comp, ht_destroy_item_fn destroy_item)
{
	hash_table_t* ht = (hash_table_t*)malloc(sizeof(hash_table_t));
	memset(ht, 0, sizeof(hash_table_t));
	
	ht->hash = hash;
	ht->key_comp = key_comp;
	ht->destory_item = destroy_item;
	ht->sz = sz;
	ht->table = (ht_node_t**)malloc(sizeof(ht_node_t*) * ht->sz);
	memset(ht->table, 0, sizeof(ht_node_t*) * ht->sz);
	return ht;
}

void ht_destroy(hash_table_t* ht)
{
	for (uint32_t i = 0;i < ht->sz; i++)
	{
		ht_node_t* node = ht->table[i];

		while (node)
		{
			ht_node_t* next = node->next;
			ht->destory_item(node->key, node->val);
			free(node);
			node = next;
		}
	}

	free(ht->table);
	free(ht);
}

void ht_insert(hash_table_t* ht, void* key, void* val)
{
	uint32_t h = ht->hash(key) % ht->sz;

	ht_node_t* node = (ht_node_t*)malloc(sizeof(ht_node_t));
	node->key = key;
	node->val = val;
	node->next = NULL;
	if (!ht->table[h])
	{
		ht->table[h] = node;
	}
	else
	{
		node->next = ht->table[h];
		ht->table[h] = node;
	}
}

bool ht_contains(hash_table_t* ht, void* key)
{
	uint32_t h = ht->hash(key) % ht->sz;
	ht_node_t* node = ht->table[h];

	while (node)
	{
		if (ht->key_comp(key, node->key))
			return true;
		node = node->next;
	}
	return false;
}

void* ht_lookup(hash_table_t* ht, void* key)
{
	uint32_t h = ht->hash(key) % ht->sz;
	ht_node_t* node = ht->table[h];

	while (node)
	{
		if (ht->key_comp(key, node->key))
			return node->val;
		node = node->next;
	}
	return NULL;
}

bool ht_empty(hash_table_t* ht)
{
	for (uint32_t i = 0; i < ht->sz; i++)
	{
		if (ht->table[i])
			return false;
	}
	return true;
}

uint32_t ht_count(hash_table_t* ht)
{
	uint32_t nodes = 0;
	for (uint32_t i = 0; i < ht->sz; i++)
	{
		ht_node_t* node = ht->table[i];
		while (node)
		{
			nodes++;
			node = node->next;
		}
	}
	return nodes;
}

bool ht_remove(hash_table_t* ht, void* key)
{
	uint32_t h = ht->hash(key) % ht->sz;
	
	if (!ht->table[h]) return false;

	bool found = false;
	ht_node_t* node = ht->table[h];
	ht_node_t* new_list = NULL;
	while (node)
	{
		ht_node_t* next = node->next;
		if (ht->key_comp(key, node->key))
		{
			ht->destory_item(node->key, node->val);
			free(node);
			found = true;
		}
		else
		{
			node->next = new_list;
			new_list = node;
		}
		node = next;
	}
	ht->table[h] = new_list;
	return found;
}

ht_iterator_t ht_begin(hash_table_t* ht)
{
	ht_iterator_t result = { 0, NULL };
	for (uint32_t i = 0; i < ht->sz; i++)
	{
		if (ht->table[i])
		{
			result.index = i;
			result.node = ht->table[i];
			return result;
		}
	}
	return result;
}

bool ht_end(hash_table_t* ht, ht_iterator_t* it)
{
	ht;
	return it->node == NULL;
}

bool ht_next(hash_table_t* ht, ht_iterator_t* it)
{
	it->node = it->node->next;

	if (it->node == NULL)
	{
		do
		{
			it->index++;
			if (it->index == ht->sz)
			{
				it->node = NULL;
				break;
			}
			it->node = ht->table[it->index];
		} while (it->node == NULL);
	}
	return it->node != NULL;
}

void ht_print_stats(hash_table_t* ht)
{
	uint32_t nodes = 0;
	uint32_t max_depth = 0;

	for (uint32_t i = 0; i < ht->sz; i++)
	{
		ht_node_t* node = ht->table[i];

		uint32_t depth = 0;
		while (node)
		{
			nodes++;
			depth++;
			node = node->next;
		}
		if (depth > max_depth)
			max_depth = depth;
	}
	printf("HT\tnodes: %d max depth: %d", nodes, max_depth);
}

/* string hash table */

static size_t _sht_hash(void* key)
{
	const char* s = (const char*)key;
	size_t hashval;

	for (hashval = 0; *s != '\0'; s++)
		hashval = *s + 31 * hashval;
	return hashval;
}

static bool _sht_comp(void* lhs, void* rhs)
{
	return strcmp((const char*)lhs, (const char*)rhs) == 0;
}

static void _sht_destroy_item(void* key, void* val)
{
	val;
	free(key);
}

hash_table_t* sht_create(uint32_t sz)
{
	return ht_create(sz, &_sht_hash, &_sht_comp, &_sht_destroy_item);
}

void sht_insert(hash_table_t* ht, const char* key, void* val)
{
	char* keydup = strdup(key);
	ht_insert(ht, keydup, val);
}

bool sht_contains(hash_table_t* ht, const char* key)
{
	return ht_contains(ht, (void*)key);
}

bool sht_remove(hash_table_t* ht, const char* key)
{
	return ht_remove(ht, (void*)key);
}

void* sht_lookup(hash_table_t* ht, const char* key)
{
	return ht_lookup(ht, (void*)key);
}

sht_iterator_t sht_begin(hash_table_t* ht)
{
	sht_iterator_t result;
	result.it = ht_begin(ht);
	if (result.it.node)
	{
		result.key = (const char*)result.it.node->key;
		result.val = result.it.node->val;
	}
	else
	{
		result.key = NULL;
		result.val = NULL;
	}
	return result;
}

bool sht_end(hash_table_t* ht, sht_iterator_t* it)
{
	return ht_end(ht, &it->it);
}

bool sht_next(hash_table_t* ht, sht_iterator_t* it)
{
	if (!ht_next(ht, &it->it)) return false;

	it->key = (const char*)it->it.node->key;
	it->val = it->it.node->val;
	return true;
}

/* pointer hash set */
static size_t _phs_hash(void* key)
{
	return (size_t)key;
}

static bool _phs_comp(void* lhs, void* rhs)
{
	return lhs == rhs;
}

static void _phs_destroy_item(void* key, void* val)
{
	key = val = 0;
}

hash_table_t* phs_create(uint32_t sz)
{
	return ht_create(sz, &_phs_hash, &_phs_comp, &_phs_destroy_item);
}

void phs_insert(hash_table_t* ht, void* ptr)
{
	ht_insert(ht, ptr, (void*)1);
}

phs_iterator_t phs_begin(hash_table_t* ht)
{
	phs_iterator_t result;
	result.it = ht_begin(ht);
	if (result.it.node)
	{
		//key in val in a set
		result.val = result.it.node->key;
	}
	else
	{
		result.val = NULL;
	}
	return result;
}

bool phs_end(hash_table_t* ht, phs_iterator_t* it)
{
	return ht_end(ht, &it->it);
}

bool phs_next(hash_table_t* ht, phs_iterator_t* it)
{
	if (!ht_next(ht, &it->it)) return false;

	//key in val in a set
	it->val = it->it.node->key;
	return true;
}
