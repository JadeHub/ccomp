#pragma once

#include <stdbool.h>

typedef struct
{
	bool valid;
	bool display_version;
	bool pre_proc_only;

	char* output_path;
	char* input_path;

}comp_opt_t;

comp_opt_t parse_command_line(int argc, const char* argv[]);