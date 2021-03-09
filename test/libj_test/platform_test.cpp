#include <gtest/gtest.h>
#include <gmock/gmock.h>

extern "C"
{
#include <libj/include/platform.h>
}

TEST(Platform, path)
{
	const char* path = "file.txt";
	const char* cur_path = path_dirname(path);
}