#include "str_buff.h"

#include <string.h>

static void _expand_buff(str_buff_t* sb)
{
	sb->sz *= 2;
	char* buff = (char*)malloc(sb->sz);
	strcpy(buff, sb->buff);
	free(sb->buff);
	sb->buff = buff;
}

str_buff_t* sb_create(size_t sz)
{
	str_buff_t* result = (str_buff_t*)malloc(sizeof(str_buff_t));
	result->sz = sz;
	result->len = 0;
	result->buff = (char*)malloc(sz);
	result->buff[0] = '\0';
	return result;
}

//create a string buffer containing the supplied param
str_buff_t* sb_attach(char* buff, size_t sz)
{
	str_buff_t* result = (str_buff_t*)malloc(sizeof(str_buff_t));
	result->sz = sz;
	result->len = strlen(buff);
	result->buff = buff;
	return result;
}

char* sb_release(str_buff_t* sb)
{
	if (!sb)
		return NULL;
	char* result = sb->buff;
	free(sb);
	return result;
}

void sb_destroy(str_buff_t* sb)
{
	free(sb_release(sb));
}

char* sb_append_ch(str_buff_t* sb, const char ch)
{
	static char tmp[2];
	tmp[0] = ch;
	tmp[1] = '\0';
	return sb_append(sb, tmp);
}

char* sb_append(str_buff_t* sb, const char* str)
{
	size_t len = strlen(str);
	while (sb->len + len >= sb->sz)
		_expand_buff(sb);
	strcat(sb->buff, str);
	sb->len += len;
	return sb->buff;
}

//append int and return internal buffer
char* sb_append_int(str_buff_t* sb, int64_t val, int base)
{
	static const char* digits = "0123456789ABCDEF";

	char str[64];
	char* p = str;

	uint64_t counter = val;
	do
	{
		p++;
		counter = counter / base;

	} while (counter);
	*p = '\0';
	do
	{
		*--p = digits[val % base];
		val = val / base;

	} while (val);

	return sb_append(sb, str);
}

char* sb_str(str_buff_t* sb)
{
	return sb->buff;
}