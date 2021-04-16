
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>
#include <libcomp/include/token.h>
#include <libcomp/include/lexer.h>
#include <libcomp/include/pp.h>
#include <libcomp/include/parse.h>
#include <libcomp/include/code_gen.h>
#include <libcomp/include/sema.h>
#include "libcomp/include/comp_opt.h"

#include <libj/include/platform.h>

void asm_print(const char* line, bool lf, void* data)
{
    data;
    printf("%s", line);
    if (lf)
        printf("\n");
}

void diag_err_print(token_t* tok, uint32_t err, const char* msg, void* data)
{
    data;
    fprintf(stderr, "%s(%s): Err %d: %s\n",
        src_file_path(tok->loc),
        src_file_pos_str(src_file_position(tok->loc)),
        err, msg);    
    exit(1); 
}

source_range_t _file_loader(const char* dir, const char* file, void* data)
{
    data;
    const char* path = path_combine(dir, file);

 //   printf("opening %s dir %s file %s\n", path, dir, file);

    source_range_t result = {NULL, NULL};
    FILE* f = fopen(path, "rb");
    if (f)
    {
        fseek(f, 0, SEEK_END);
        unsigned long len = (unsigned long)ftell(f);
        fseek(f, 0, SEEK_SET);
        char* buff = (char*)malloc(len + 1);
        size_t b = fread(buff, 1, len, f);
        if (b == len)
        {
            buff[len] = '\0';
            result.ptr = buff;
            result.end = buff + len;
        }
        fclose(f);
    }
    free((void*)path);
    return result;
}

char* trim(char* str)
{
    if (!str) return str;

    while (isspace(*str))
        str++;

    while (isspace(str[strlen(str) - 1]))
        str[strlen(str) - 1] = '\0';

    return str;
}

bool load_config(const char* path)
{
    FILE* f = fopen(path, "r");
    if (!f)
        return false;

    char line[1024];

    while(fgets(line, sizeof(line), f))
    {
        char* eq = strchr(line, '=');
        if (!eq)
            return false;
        *eq = '\0';
        eq++;
        eq = trim(eq);
        if (strcmp(line, "inc_path") == 0 && strlen(eq))
            src_add_header_path(eq);
    }

    fclose(f);

    return true;
}

int main(int argc, char* argv[])
{
    diag_set_handler(&diag_err_print, NULL);

    comp_opt_t options = parse_command_line(argc, argv);

    if (!options.valid)
    {
        puts("invalid parameters");
        return 0;
    }

    if (options.display_version)
    {
        puts("jcompiler version 0.1");
        return 0;
    }

    const char* path = path_resolve(options.input_path);
    const char* src_dir = path_dirname(path);
    const char* src_file = path_filename(path);
    free((void*)path);
        
    if (!src_file || !src_dir)
        return -1;

    src_init(&_file_loader, NULL);

    if (options.config_path && !load_config(options.config_path))
        return -1;

    lex_init();

    source_range_t* sr = src_load_file(src_dir, src_file);
    free((void*)src_file);
    
    if (!sr)
        return -1;
    
    token_range_t range = lex_source(sr);
    if (!range.start)
        return -1;

    pre_proc_init();
    token_range_t preproced = pre_proc_file(src_dir, &range);
    pre_proc_deinit();

    if (options.pre_proc_only)
    {
        tok_range_print(&preproced);
        return 0;
    }

    parse_init(preproced.start);
    ast_trans_unit_t* ast = parse_translation_unit();
    valid_trans_unit_t* tl = sem_analyse(ast);
    if (!tl)
    {
        printf("Failed to validate\n");
        return -1;
    }
    code_gen(tl, &asm_print, NULL, options.annotate_asm);
    tl_destroy(tl);
    
    src_deinit();
    return 0;
}

