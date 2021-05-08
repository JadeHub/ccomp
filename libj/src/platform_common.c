#include "platform.h"

#include <stdlib.h>
#include <string.h>

char* path_convert_slashes(char* path)
{
	//convert backslashes to forward slashes
	char* c = path;
	while (*c)
	{
		if (*c == '\\')
			*c = '/';
		c++;
	}
	return path;
}

const char* path_combine(const char* dir, const char* file)
{
	char* buff = (char*)malloc(strlen(dir) + strlen(file) + 2);

	strcpy(buff, dir);
	buff = path_convert_slashes(buff);

	if (buff[strlen(buff) - 1] != '/')
		strcat(buff, "/");
	strcat(buff, file);
	return buff;
}