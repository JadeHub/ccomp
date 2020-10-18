#pragma once

#ifdef __GNUC__
#define PLATFORM_LINUX
#else
#define PLATFORM_WIN
#endif

char* path_resolve(const char* path);
char* path_dirname(const char* path);
char* path_filename(const char* path);
char* path_combine(const char* dir, const char* file);