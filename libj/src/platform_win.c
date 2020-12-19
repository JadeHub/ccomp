#include "platform.h"

#ifdef PLATFORM_WIN

#include <windows.h>

#include <stdlib.h>

#include <fileapi.h>

const char* path_resolve(const char* path)
{
	char* buff = (char*)malloc(MAX_PATH + 1);
	if (GetFullPathNameA(path, MAX_PATH + 1, buff, NULL) == 0)
	{
		free(buff);
		buff = NULL;
	}
	return path_convert_slashes(buff);
}

const char* path_dirname(const char* path)
{
	char* buff = (char*)malloc(MAX_PATH + 1);
	char* file_part;
	if (GetFullPathNameA(path, MAX_PATH + 1, buff, &file_part) == 0)
	{
		free(buff);
		buff = NULL;
	}
	*file_part = '\0';
	return path_convert_slashes(buff);
}

const char* path_filename(const char* path)
{
	char buff[MAX_PATH + 1];
	char* file_part;
	if (GetFullPathNameA(path, MAX_PATH+1, buff, &file_part) == 0)
		return NULL;
	char* fn = (char*)malloc(strlen(file_part) + 1);
	strcpy(fn, file_part);
	return fn;
}

#endif
