#pragma once

#include <stdlib.h>
#include <stdint.h>

typedef struct
{
	char* buff;
	size_t sz;
	size_t len;
}str_buff_t;

//create an empty string buffer of size
str_buff_t* sb_create(size_t sz);

//create a string buffer containing the supplied param
str_buff_t* sb_attach(char* buff, size_t sz);

//append str and return internal buffer
char* sb_append(str_buff_t* sb, const char* str);

char* sb_str(str_buff_t* sb);

//append int and return internal buffer
char* sb_append_int(str_buff_t* sb, int64_t val, int base);

//append single character and return internal buffer
char* sb_append_ch(str_buff_t* sb, const char ch);

//destroy string buff object and return internal buffer
char* sb_release(str_buff_t* sb);

//destroy string buff object and internal buffer
void sb_destroy(str_buff_t* sb);
