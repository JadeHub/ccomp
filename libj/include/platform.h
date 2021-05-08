#pragma once

#ifdef __GNUC__
#define PLATFORM_LINUX
#else
#define PLATFORM_WIN
#endif

const char* path_resolve(const char* path);
const char* path_dirname(const char* path);
const char* path_filename(const char* path);
const char* path_combine(const char* dir, const char* file);
char* path_convert_slashes(char* path);