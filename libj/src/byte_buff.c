#include "byte_buff.h"

#include <string.h>
#include <assert.h>

static void _expand_buff(byte_buff_t* bb)
{
	size_t new_sz = bb->sz * 2;
	
	uint8_t* buff = (uint8_t*)malloc(new_sz);
	memset(buff, 0, new_sz);
	memcpy(buff, bb->buff, bb->sz);
	free(bb->buff);
	bb->sz = new_sz;
	bb->buff = buff;
}

byte_buff_t* bb_create(size_t sz)
{
	byte_buff_t* result = (byte_buff_t*)malloc(sizeof(byte_buff_t));
	memset(result, 0, sizeof(byte_buff_t));

	result->sz = sz;
	result->buff = (uint8_t*)malloc(sz);
	memset(result->buff, 0, sz);
	return result;
}

void bb_append(byte_buff_t* bb, uint8_t val)
{
	assert(bb->len <= bb->sz);

	if (bb->len == bb->sz)
		_expand_buff(bb);

	bb->buff[bb->len] = val;
	bb->len++;
}

uint8_t* bb_release(byte_buff_t* bb)
{
	uint8_t* result = bb->buff;
	free(bb);
	return result;
}

//destroy string buff object and internal buffer
void bb_destroy(byte_buff_t* bb)
{
	free(bb->buff);
	free(bb);
}
