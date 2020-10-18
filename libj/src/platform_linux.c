#include "platform.h"

#ifdef PLATFORM_LINUX

#include <stdlib.h>
#include <string.h>
#include <libgen.h>

char* path_resolve(const char* path)
{
	return realpath(path, NULL);
}

char* path_dirname(const char* path)
{
	char* buff = (char*)malloc(strlen(path) + 1);
	strcpy(buff, path);
	buff = dirname(buff);
	return buff;
}

char* path_filename(const char* path)
{
	char* buff = (char*)malloc(strlen(path) + 1);
	strcpy(buff, path);
	buff = basename(buff);
	return buff;
}

#endif