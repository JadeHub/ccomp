#include "platform.h"

#include <stdlib.h>

#ifdef PLATFORM_WIN

#pragma warning (push)

//warning C4668: '_WIN32_WINNT_WIN10_RS4' is not defined as a preprocessor macro, replacing with '0' for '#if/#elif'
#pragma warning (disable : 4668)
//warning C4255: 'EnableMouseInPointerForThread': no function prototype given: converting '()' to '(void)'
#pragma warning (disable : 4255)

#include <windows.h>

#pragma warning (pop)


#include <fileapi.h>

char* path_resolve(const char* path)
{
	char* buff = (char*)malloc(MAX_PATH + 1);
	if (GetFullPathNameA(path, MAX_PATH + 1, buff, NULL) == 0)
	{
		free(buff);
		buff = NULL;
	}
	return buff;
}

char* path_dirname(const char* path)
{
	char* buff = (char*)malloc(MAX_PATH + 1);
	char* file_part;
	if (GetFullPathNameA(path, MAX_PATH + 1, buff, &file_part) == 0)
	{
		free(buff);
		buff = NULL;
	}
	*file_part = '\0';
	return buff;
}

char* path_filename(const char* path)
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


char* path_combine(const char* dir, const char* file)
{
	char* buff = (char*)malloc(strlen(dir) + strlen(file) + 2);
	strcpy(buff, dir);
	strcat(buff, "/");
	strcat(buff, file);
	return buff;
}