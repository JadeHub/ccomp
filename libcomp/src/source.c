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
	char* path;
}source_file_t;

/*
Map of path to source_file_t* for each open file

We can find the file associated with a char pointer by looking at the source ranges of each file
*/
static hash_table_t* _files;
static char* _src_dir = NULL;
static src_load_cb _load_cb;
static void* _load_data;

static int _is_line_ending(const char* ptr)
{
	if (*ptr == '\r')
	{
		return 1;
	}
	else if (*ptr == '\n')
	{
		return *(ptr++) == '\r' ? 2 : 1;
	}
	return 0;
}

static uint32_t _count_lines(source_range_t* range)
{
	const char* ptr = range->ptr;
	uint32_t result = 0;
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
	int line = 0;
	while (ptr < f->range.end)
	{
		int adv = _is_line_ending(ptr);
		if (adv)
		{
			f->line_count++;
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

const char* src_file_path(const char* pos)
{
	source_file_t* file = _get_file_for_pos(pos);
	return file ? file->path : NULL;
}

file_pos_t src_file_position(const char* pos)
{
	source_file_t* file = _get_file_for_pos(pos);
	file_pos_t result;
	result.col = 0;
	result.line = 0;
	uint32_t line = 0;
	for (line = 0; line < file->line_count; line++)
	{
		if (pos < file->lines[line])
		{
			if (line == 0)
			{
				result.line = 1;
				result.col = (pos - file->range.ptr) + 1;
			}
			else
			{
				result.line = line + 1;
				result.col = pos - file->lines[line-1] + 1;
			}
			return result;
		}
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

const char* src_file_pos_str(file_pos_t pos)
{
	static char _buff[32];
	sprintf(_buff, "Ln: %d Ch: %d", pos.line, pos.col);
	return _buff;
}

static bool _is_white_space(const char c)
{
	return c == ' ' ||
		c == '\t' ||
		c == '\n' ||
		c == '\r';
}

char* src_extract(const char* start, const char* end)
{
	size_t len = end - start;
	char* result = (char*)malloc(len + 1);
	strncpy(result, start, len);
	result[len] = '\0';

	while (len > 0 && _is_white_space(result[len - 1]))
	{
		result[len - 1] = '\0';
		len--;
	}	

	return result;
}

bool src_is_valid_range(source_range_t* src)
{
	return src && src->ptr && src->end && src->ptr < src->end;
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

source_range_t* src_load_header(const char* fn, include_kind kind)
{
	assert(_src_dir);
	
	source_file_t* file = _load_file(_src_dir, fn);
	//search different paths...
	return file ? &file->range : NULL;
}

source_range_t* src_load_file(const char* fn)
{
	assert(_src_dir);
	
	source_file_t* file = _load_file(_src_dir, fn);
	return file ? &file->range : NULL;
}

void src_init(const char* src_path, src_load_cb load_cb, void* load_data)
{
	_src_dir = path_resolve(src_path);
	_files = sht_create(32);
	_load_cb = load_cb;
	_load_data = load_data;
}

void src_deinit()
{
	free(_src_dir);
	sht_iterator_t it = sht_begin(_files);
	while (!sht_end(_files, &it))
	{
		source_file_t* sf = (source_file_t*)it.val;
		free(sf->path);
		free(sf);
		sht_next(_files, &it);
	}
	ht_destroy(_files);
}