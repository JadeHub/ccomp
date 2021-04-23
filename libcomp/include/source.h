#pragma once

//source code management

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/*
A range of memory containing source code
*/
typedef struct
{
    const char* ptr;
    const char* end;
}source_range_t;

/*
Information about a single position within a source file
*/
typedef struct
{
    uint32_t line;
    uint16_t col;
    const char* path;
}file_pos_t;

/*
Include type - <blah.h> or "blah.h"
*/
typedef enum
{
    include_local,
    include_system
}include_kind;

/*
Callback used to load source file into memory
*/
typedef source_range_t (*src_load_cb)(const char* dir, const char* file, void* data);

/*
Set the callback used to load source files into memory
*/
void src_init(src_load_cb load_cb, void* load_data);

/*
Destroy data structures
*/
void src_deinit();

/*
Add a path to be searched when loading a header
*/
void src_add_header_path(const char* path);

/*
Return info (path, line & column) about a given source char
*/
file_pos_t src_get_pos_info(const char* pos);

/*
Load a source file
*/
source_range_t* src_load_file(const char* path, const char* file_name);

/*
Attempt to load a header file
*/
source_range_t* src_load_header(const char* cur_dir, const char* path, include_kind kind);

/*
Return true if the given source_range_t is valid
*/
bool src_is_valid_range(source_range_t* src);

//used for test code
void src_register_range(source_range_t range, char* file);
