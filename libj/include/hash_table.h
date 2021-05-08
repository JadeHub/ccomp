#pragma once

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

typedef size_t (*ht_hash_fn)(void*);
typedef bool (*ht_key_comp_fn)(void*, void*);
typedef void (*ht_destroy_item_fn)(void*, void*);

typedef struct ht_node
{
	void* key;
	void* val;
	struct ht_node* next;
}ht_node_t;

typedef struct
{
	ht_hash_fn hash;
	ht_key_comp_fn key_comp;
	ht_destroy_item_fn destory_item;

	uint32_t sz;
	ht_node_t** table;
}hash_table_t;

typedef struct
{
	uint32_t index;
	ht_node_t* node;
}ht_iterator_t;

hash_table_t* ht_create(uint32_t sz, ht_hash_fn hash, ht_key_comp_fn key_comp, ht_destroy_item_fn destroy_item);
void ht_destroy(hash_table_t* ht);
void ht_insert(hash_table_t* ht, void* key, void* item);
bool ht_contains(hash_table_t* ht, void* key);
void* ht_lookup(hash_table_t* ht, void* key);
bool ht_empty(hash_table_t* ht);
uint32_t ht_count(hash_table_t* ht);
bool ht_remove(hash_table_t* ht, void* key);
ht_iterator_t ht_begin(hash_table_t* ht);
bool ht_end(hash_table_t* ht, ht_iterator_t* it);
bool ht_next(hash_table_t* ht, ht_iterator_t* it);
void ht_print_stats(hash_table_t* ht);

/* string hash table */
typedef struct
{
	const char* key;
	void* val;
	ht_iterator_t it;
}sht_iterator_t;

hash_table_t* sht_create(uint32_t sz);
void sht_insert(hash_table_t* ht, const char* key, void* val);
bool sht_contains(hash_table_t* ht, const char* key);
bool sht_remove(hash_table_t* ht, const char* key);
void* sht_lookup(hash_table_t* ht, const char* key);
sht_iterator_t sht_begin(hash_table_t* ht);
bool sht_end(hash_table_t* ht, sht_iterator_t* it);
bool sht_next(hash_table_t* ht, sht_iterator_t* it);

/* pointer hash set */
typedef struct
{
	void* val;
	ht_iterator_t it;
}phs_iterator_t;

hash_table_t * phs_create(uint32_t sz);
void phs_insert(hash_table_t* ht, void* ptr);
phs_iterator_t phs_begin(hash_table_t* ht);
bool phs_end(hash_table_t* ht, phs_iterator_t* it);
bool phs_next(hash_table_t* ht, phs_iterator_t* it);
