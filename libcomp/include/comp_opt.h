#pragma once

#include <stdbool.h>

typedef struct
{
	bool valid;
	bool display_version;
	bool pre_proc_only;

	bool annotate_asm;

	char* output_path;
	char* input_path;
	char* config_path;

}comp_opt_t;

comp_opt_t parse_command_line(int argc,	char* argv[]);