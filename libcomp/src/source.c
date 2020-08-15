#include "source.h"

#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct
{
	source_range_t range;
	const char** lines;
	uint32_t line_count;
}source_file_t;

source_file_t _theFile;

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

static void _init_line_data(source_file_t* f)
{
	const char* ptr = f->range.ptr;

	f->line_count = 0;
	while (ptr < f->range.end)
	{
		int adv = _is_line_ending(ptr);
		if (adv)
		{
			f->line_count++;
			ptr += adv;
		}
		else
		{
			ptr++;
		}
	}

	f->lines = (const char**)malloc(sizeof(const char*) * f->line_count);
	ptr = f->range.ptr;
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

file_pos_t src_file_position(const char* pos)
{
	source_file_t* f = &_theFile;
	file_pos_t result;
	result.col = 0;
	result.line = 0;
	uint32_t line = 0;
	for (line = 0; line < f->line_count; line++)
	{
		if (pos < f->lines[line])
		{
			if (line == 0)
			{
				result.line = 1;
				result.col = (pos - f->range.ptr) + 1;
			}
			else
			{
				result.line = line + 1;
				result.col = pos - _theFile.lines[line-1] + 1;
			}
			return result;
		}
	}
	return result;
}

source_range_t* src_init_source(const char* src, size_t len)
{
	source_file_t* f = &_theFile;

	size_t buff_len = len;
	char* buff;
	if (src[buff_len - 1] != '\0')
		buff_len++;
	buff = (char*)malloc(buff_len);
	buff[buff_len - 1] = '\0';
	strncpy(buff, src, len);
	f->range.ptr = buff;
	f->range.end = buff + buff_len;
	_init_line_data(f);
	return &f->range;
}

const char* src_file_pos_str(file_pos_t pos)
{
	static char _buff[32];
	sprintf(_buff, "%d, %d", pos.line, pos.col);
	return _buff;
}
