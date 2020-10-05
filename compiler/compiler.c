
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <libcomp/include/token.h>
#include <libcomp/include/lexer.h>
#include <libcomp/include/parse.h>
#include <libcomp/include/code_gen.h>
#include <libcomp/include/sema.h>

static const char* _src = "int main() { struct A {int b; int c; }a; a.c = 5; return a.c;}";

void print_tokens(token_t* toks)
{
    token_t* tok = toks;

    while (tok)
    {
        tok_printf(tok);
        tok = tok->next;
    }
}

void asm_print(const char* line, void* data)
{
    if(line[0] != '\n' && line[0] != '#')
        printf("%s\n", line);
}

void diag_err_print(token_t* tok, uint32_t err, const char* msg, void* data)
{
    fprintf(stderr, "%s(%s): Err %d: %s\n",
        "file.c",
        diag_pos_str(tok),
        err, msg);
    exit(1); 
}

source_range_t _file_loader(const char* path, void* d)
{
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
            result.ptr = buff;
            result.end = buff + len;
        }
        fclose(f);
    }
    return result;
}

int main(int argc, char* argv[])
{
    src_init(&_file_loader, NULL);
    lex_init();
    diag_set_handler(&diag_err_print, NULL);

    token_t* toks;

    if (argc == 2)
    {
        source_range_t* sr = src_load_file(argv[1]);
        if (!sr)
            return -1;
        toks = lex_source(sr);
    }
    else
    {
        source_range_t sr;
        sr.ptr = _src;
        sr.end = sr.ptr + strlen(_src);
        toks = lex_source(&sr);
    }

  
    ast_trans_unit_t* ast = parse_translation_unit(toks);
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

