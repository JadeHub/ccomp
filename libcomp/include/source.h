#pragma once

#include <stddef.h>
#include <stdint.h>

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

file_pos_t src_file_position(const char* pos);
source_range_t* src_init_source(const char* src, size_t len);
const char* src_file_pos_str(file_pos_t);
char* src_extract(const char* start, const char* end);
