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
	//dirname() may modify its input so give it a copy
	char* path_cpy = strdup(path);
	char* tmp = dirname(path_cpy);
	if (!tmp)
	{
		free(path_cpy);
		return NULL;
	}
	char* result = strdup(tmp);
	free(path_cpy);
	return result;
}

char* path_filename(const char* path)
{
	char* fn = basename(path);
	char* buff = (char*)malloc(strlen(fn) + 1);
	strcpy(buff, fn);
	return buff;
}

#endif