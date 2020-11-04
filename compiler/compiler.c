
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <libcomp/include/token.h>
#include <libcomp/include/lexer.h>
#include <libcomp/include/pp.h>
#include <libcomp/include/parse.h>
#include <libcomp/include/code_gen.h>
#include <libcomp/include/sema.h>

#include <libj/include/platform.h>

void asm_print(const char* line, void* data)
{
    if(line[0] != '\n' && line[0] != '#')
        printf("%s\n", line);
}

void diag_err_print(token_t* tok, uint32_t err, const char* msg, void* data)
{
    fprintf(stderr, "%s(%s): Err %d: %s\n",
        src_file_path(tok->loc),
        diag_pos_str(tok),
        err, msg);    
    exit(1); 
}

source_range_t _file_loader(const char* dir, const char* file, void* d)
{
    char* path = path_combine(dir, file);

 //   printf("opening %s dir %s file %s\n", path, dir, file);

    source_range_t result = {NULL, NULL};
    FILE* f = fopen(path, "r");
    if (f)
    {
        fseek(f, 0, SEEK_END);
        unsigned long len = (unsigned long)ftell(f);
        fseek(f, 0, SEEK_SET);
        char* buff = (char*)malloc(len + 1);
        if (fread(buff, 1, len, f) == len)
        {
            buff[len] = '\0';
            result.ptr = buff;
            result.end = buff + len;
        }
        fclose(f);
    }
    free(path);
    return result;
}

int main(int argc, char* argv[])
{
    diag_set_handler(&diag_err_print, NULL);

    if (argc != 2)
        return -1;
    
    char* path = path_resolve(argv[1]);
    char* src_dir = path_dirname(path);
    char* src_file = path_filename(path);
    free(path);
        
    if (!src_file || !src_dir)
        return -1;

    src_init(src_dir, &_file_loader, NULL);
    lex_init();

    source_range_t* sr = src_load_file(src_file);
    free(src_file);
    free(src_dir);
    if (!sr)
        return -1;
    
    token_range_t range = lex_source(sr);
    if (!range.start)
        return -1;
    
    pre_proc_init();
    token_range_t preproced = pre_proc_file(range.start);
    pre_proc_deinit();

    ast_trans_unit_t* ast = parse_translation_unit(preproced.start);
    valid_trans_unit_t* tl = sem_analyse(ast);
    if (!tl)
    {
        printf("Failed to validate\n");
        return -1;
    }
    code_gen(tl, &asm_print, NULL);
    tl_destroy(tl);
    
    src_deinit();
    return 0;
}

