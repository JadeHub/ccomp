#pragma once

#include <stdint.h>
#include <stdlib.h>

typedef struct
{
	uint8_t* buff;
	size_t sz;
	size_t len;
}byte_buff_t;

//create an empty string buffer of size
byte_buff_t* bb_create(size_t sz);

void bb_append(byte_buff_t* bb, uint8_t val);

uint8_t* bb_release(byte_buff_t* bb);

//destroy string buff object and internal buffer
void bb_destroy(byte_buff_t* bb);
