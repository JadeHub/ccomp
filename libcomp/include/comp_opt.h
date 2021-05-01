#pragma once

//compiler options

#include <stdbool.h>

typedef struct
{
	/*
	true if the params have been succesfully parsed
	*/
	bool valid;

	/*
	user requests display of the version number
	*/
	bool display_version;

	/*
	run the preprocessor only and print the output
	*/
	bool pre_proc_only;

	/*
	annotate the assembly output
	*/
	bool annotate_asm;

	/*
	assembly output path
	*/
	char* output_path;

	/*
	source input path
	*/
	char* input_path;

	/*
	config file path
	*/
	char* config_path;

	/*
	* dump type info
	*/
	bool dump_type_info;

}comp_opt_t;

/*
parse the command line
*/
comp_opt_t parse_command_line(int argc, char* argv[]);