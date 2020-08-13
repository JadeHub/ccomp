#pragma once

#include <stdbool.h>

struct ptr_set;

struct ptr_set* ps_create();
void ps_destroy(struct ptr_set* set);
void ps_insert(struct ptr_set* set, void* ptr);
bool ps_remove(struct ptr_set* set, void* ptr);
void* ps_lookup(struct ptr_set* set, void*);