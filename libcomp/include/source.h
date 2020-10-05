#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct
{
    const char* ptr;
    const char* end;
}source_range_t;

typedef struct
{
    uint16_t line;
    uint16_t col;
}file_pos_t;

/*
Load source file into memory. Must be terminated with '\0'
*/
typedef source_range_t (*src_load_cb)(const char* path, void* data);

void src_init(src_load_cb load_cb, void* load_data);
void src_deinit();
file_pos_t src_file_position(const char* pos);
const char* src_file_pos_str(file_pos_t);
char* src_extract(const char* start, const char* end);
source_range_t* src_load_file(const char* path);
bool src_is_valid_range(source_range_t* src);
