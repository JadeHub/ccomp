#pragma once

#include <stdbool.h>

struct named_ptr_set;

struct named_ptr_set* nps_create();
void nps_destroy(struct named_ptr_set* set);
void nps_insert(struct named_ptr_set* set, const char* name, void* ptr);
bool nps_remove(struct named_ptr_set* set, const char* name);
void* nps_lookup(struct named_ptr_set* set, const char* name);