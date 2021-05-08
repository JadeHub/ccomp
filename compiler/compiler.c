
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
#include <libcomp/include/abi.h>

#include <libj/include/platform.h>
#include <libj/include/str_buff.h>

static comp_opt_t options;

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

    file_pos_t fp = src_get_pos_info(tok->loc);

    fprintf(stderr, "%s(Ln: %d Ch: %d): Err %d: %s\n",
        fp.path ? fp.path : "unknown",
        fp.line, fp.col,
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

void on_observe_user_type_def(ast_type_spec_t* spec)
{
    if (options.dump_type_info && spec->data.user_type_spec->kind != user_type_enum)
    {
        printf("User type %s\n", spec->data.user_type_spec->name);
        ast_struct_member_t* member = spec->data.user_type_spec->data.struct_members;
        while (member)
        {
            str_buff_t* type_name = sb_create(128);
            ast_decl_type_describe(type_name, member->decl);

            if (ast_is_bit_field_member(member))
            {
                printf("\t Offset %3ld (bits %2ld:%2ld) Member %-32s %s:%ld\n", 
                    member->sema.offset,
                    member->sema.bit_field.offset,
                    member->sema.bit_field.offset + member->sema.bit_field.size,
                    member->decl->name,
                    sb_str(type_name),
                    member->sema.bit_field.size);
            }
            else
            {
                printf("\t Offset %3ld (len %3ld)    Member %-32s %s\n",
                    member->sema.offset,
                    member->decl->type_ref->spec->size,
                    member->decl->name,
                    sb_str(type_name));
            }
            sb_destroy(type_name);
            member = member->next;
        }
        printf("\t sizeof %2lu align %2lu\n", spec->size, abi_get_type_alignment(spec));
    }
}

bool run_code_gen()
{
    return !options.dump_type_info;
}

int main(int argc, char* argv[])
{
    diag_set_handler(&diag_err_print, NULL);

    options = parse_command_line(argc, argv);

    if (!options.valid)
    {
        fprintf(stderr, "invalid parameters\n");
        return -1;
    }

    if (options.display_version)
    {
        puts("jcompiler version 0.1");
        return 0;
    }

    const char* path = path_resolve(options.input_path);
    if (!path)
    {
        fprintf(stderr, "unknown file: %s\n", options.input_path);
        return -1;
    }

    const char* src_dir = path_dirname(path);
    const char* src_file = path_filename(path);
    free((void*)path);
        
    if (!src_file || !src_dir)
        return -1;

    src_init(&_file_loader, NULL);

    if (options.config_path && !load_config(options.config_path))
        return -1;

    //load file
    source_range_t* sr = src_load_file(src_dir, src_file);
    free((void*)src_file);
    if (!sr)
        return -1;

    //lex
    lex_init();
    token_range_t range = lex_source(sr);
    if (!range.start)
        return -1;

    //pre proc
    pre_proc_init();
    token_range_t preproced = pre_proc_file(src_dir, &range);
    pre_proc_deinit();

    if (options.pre_proc_only)
    {
        tok_range_print(&preproced);
        return 0;
    }

    //parse
    parse_init(preproced.start);
    ast_trans_unit_t* ast = parse_translation_unit();

    //semantic analysis
    sema_observer_t so = { &on_observe_user_type_def };
    sema_init(so);
    valid_trans_unit_t* tl = sema_analyse(ast);
    if (!tl)
        return -1;

    //code generation
    if(run_code_gen())
        code_gen(tl, &asm_print, NULL, options.annotate_asm);
    
    tl_destroy(tl);
    src_deinit();
    return 0;
}

