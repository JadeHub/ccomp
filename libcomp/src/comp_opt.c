#include "comp_opt.h"

#include <string.h>
#include <stdlib.h>

comp_opt_t parse_command_line(int argc, const char* argv[])
{
	comp_opt_t result;
	memset(&result, 0, sizeof(comp_opt_t));

	int idx = 1;

	while (idx < argc)
	{
		if (argv[idx][0] == '-')
		{
			const char* pos = &argv[idx][1];
			while (*pos)
			{
				if (*pos == 'E')
				{
					result.pre_proc_only = true;
				}
				else if (*pos == 'v')
				{
					result.display_version = true;
				}
				else if (*pos == 'a')
				{
					result.annotate_asm = true;
				}
				else if (*pos == 'd')
				{
					result.dump_type_info = true;
				}
				else if (*pos == 'o')
				{
					if (++idx == argc)
						goto _err_ret;
					result.output_path = strdup(argv[idx]);
					break;
				}
				else if (*pos == 'c')
				{
					if (++idx == argc)
						goto _err_ret;
					result.config_path = strdup(argv[idx]);
					break;
				}
				pos++;
			}
		}
		else
		{
			//input file
			if (result.input_path)
				goto _err_ret;
			result.input_path = strdup(argv[idx]);
		}
		idx++;
	}

	//Display version is always valid, otherwise require an input file
	//result.valid = result.input_path != NULL || result.display_version;

	if (result.input_path != NULL || result.display_version)
		result.valid = true;

	return result;

_err_ret:
	result.valid = false;
	free(result.output_path);
	free(result.input_path);
	return result;
}