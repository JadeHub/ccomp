#include "source.h"

#include <libj/include/hash_table.h>
#include <libj/include/platform.h>

#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

typedef struct
{
	source_range_t range;
	const char** lines;
	uint32_t line_count;
	const char* path;
}source_file_t;

/*
Map of path to source_file_t* for each open file

We can find the file associated with a char pointer by looking at the source ranges of each file
*/
static hash_table_t* _files;

/*
Include path information
*/
#define MAX_INCLUDE_DIRS 20
static const char* _include_dirs[MAX_INCLUDE_DIRS];

/*
Callback to load a file
*/
static src_load_cb _load_cb;
static void* _load_data;

static int _is_line_ending(const char* ptr)
{
	if (*ptr == '\r')
	{
		return *(++ptr) == '\n' ? 2 : 1;
	}
	else if (*ptr == '\n')
	{
		return 1;
	}
	return 0;
}

static uint32_t _count_lines(source_range_t* range)
{
	const char* ptr = range->ptr;
	uint32_t result = 1;
	while (ptr < range->end)
	{
		int adv = _is_line_ending(ptr);
		if (adv)
		{
			result++;
			ptr += adv;
		}
		else
		{
			ptr++;
		}
	}
	return result;
}

static void _init_line_data(source_file_t* f)
{
	//allocate buffer
	f->line_count = _count_lines(&f->range);
	f->lines = (const char**)malloc(sizeof(const char*) * f->line_count);

	//store pointers to the start of each line
	const char* ptr = f->range.ptr;
	int line = 1;
	f->lines[0] = ptr;
	while (ptr < f->range.end)
	{
		int adv = _is_line_ending(ptr);
		if (adv)
		{
			ptr += adv;
			f->lines[line] = ptr;
			line++;
		}
		else
		{
			ptr++;
		}
	}
}

source_file_t* _get_file_for_pos(const char* pos)
{
	sht_iterator_t it = sht_begin(_files);
	while (!sht_end(_files, &it))
	{
		source_file_t* sf = (source_file_t*)it.val;

		if (pos >= sf->range.ptr && pos <= sf->range.end)
			return sf;

		sht_next(_files, &it);
	}
	return NULL;
}

file_pos_t src_get_pos_info(const char* pos)
{
	file_pos_t result;
	result.col = 0;
	result.line = 0;
	result.path = NULL;

	source_file_t* file = _get_file_for_pos(pos);
	if (!file)
		return result;

	result.path = file->path;

	uint32_t line = 0;
	for (line = 1; line < file->line_count; line++)
	{
		if (pos < file->lines[line])
		{
			if (line == 0)
			{
				result.line = 1;
				result.col = (uint16_t)((pos - file->range.ptr) + 1);
			}
			else
			{
				result.line = line;
				result.col = (uint16_t)(pos - file->lines[line - 1] + 1);
			}
			return result;
		}
	}

	if (pos <= file->range.end)
	{
		result.line = file->line_count;
		result.col = (uint16_t)(pos - file->lines[result.line - 1] + 1);
	}

	return result;
}

static source_file_t* _init_source(source_range_t src)
{
	source_file_t* file = (source_file_t*)malloc(sizeof(source_file_t));
	memset(file, 0, sizeof(source_file_t));
	
	file->range = src;
	_init_line_data(file);
	return file;
}

static bool _is_white_space(const char c)
{
	return c == ' ' ||
		c == '\t' ||
		c == '\n' ||
		c == '\r';
}

bool src_is_valid_range(source_range_t* src)
{
	return src && src->ptr && src->end && src->ptr < src->end;
}

void src_register_range(source_range_t src, char* path)
{
	source_file_t* file = _init_source(src);
	file->path = path;
	sht_insert(_files, file->path, file);
}

source_file_t* _load_file(const char* dir, const char* fn)
{
	source_range_t src = _load_cb(dir, fn, _load_data);
	if (!src_is_valid_range(&src)) return NULL;

	source_file_t* file = _init_source(src);
	file->path = path_combine(dir, fn);
	sht_insert(_files, file->path, file);
	return file;
}

source_range_t* src_load_header(const char* cur_dir, const char* fn, include_kind kind)
{
	kind;

	source_file_t* file = NULL;
	if (kind == include_local)
	{
		file = _load_file(cur_dir, fn); //local dir first if local include
		if (file)
			return &file->range;
	}

	//search list
	int i;
	for (i = 0; i < MAX_INCLUDE_DIRS && _include_dirs[i]; i++)
	{
		file = _load_file(_include_dirs[i], fn);
		if (file)
			return &file->range;
	}
	
	if (kind != include_local)
		file = _load_file(cur_dir, fn); //local dir last if system include

	return file ? &file->range : NULL;
}

source_range_t* src_load_file(const char* path, const char* file_name)
{
	source_file_t* file = _load_file(path, file_name);
	return file ? &file->range : NULL;
}

void src_add_header_path(const char* path)
{
	for (int i = 0; i < MAX_INCLUDE_DIRS; i++)
	{
		if (!_include_dirs[i])
		{
			_include_dirs[i] = path_resolve(path);
			if (!_include_dirs[i])
				fprintf(stderr, "failed to resolve include path '%s'", path);
			return;
		}
	}
	fputs("too many include paths", stderr);
}

void src_init(src_load_cb load_cb, void* load_data)
{
	memset((void*)_include_dirs, 0, MAX_INCLUDE_DIRS * sizeof(const char*));
	_files = sht_create(32);
	_load_cb = load_cb;
	_load_data = load_data;
}

void src_deinit()
{
	sht_iterator_t it = sht_begin(_files);
	while (!sht_end(_files, &it))
	{
		source_file_t* sf = (source_file_t*)it.val;
		free((void*)sf->path);
		free(sf);
		sht_next(_files, &it);
	}
	ht_destroy(_files);
}