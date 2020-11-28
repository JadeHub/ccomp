#pragma once

#include <stdbool.h>

typedef struct
{
	bool valid;
	bool display_version;
	bool pre_proc_only;

	const char* output_path;
	const char* input_path;

}comp_opt_t;

comp_opt_t parse_command_line(int argc, char* argv[]);