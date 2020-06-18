
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <libcomp/include/token.h>
#include <libcomp/include/lexer.h>
#include <libcomp/include/parse.h>
#include <libcomp/include/code_gen.h>
#include <libcomp/include/validate.h>

//static const char* _src = "int main() \r\n{return 2;}";

static char* _src = "int sum(int a, int b) { return a + b;} int main() { int a = 0;  int i; fn(a, i);  return a; }";

void print_tokens(token_t* toks)
{
    token_t* tok = toks;

    while (tok)
    {
        tok_printf(tok);
        tok = tok->next;
    }
}

void asm_print(const char* line)
{
    printf("%s\n", line);
}

void diag_err_print(token_t* tok, uint32_t err, const char* msg, void* data)
{
    file_pos_t pos = src_file_position(tok->loc);

    printf("%s(%s): Err %d: %s\n",
        "file.c",
        diag_pos_str(tok),
        err, msg);

    //printf("%s at line %d col %d\n", msg, pos.line, pos.col);
    exit(1); 
}

int main(int argc, char* argv[])
{
    lex_init();
    diag_set_handler(&diag_err_print, NULL);

    token_t* toks;

    if (argc == 2)
    {
        FILE* f = fopen(argv[1], "r");
        if (!f)
            return 1;

        fseek(f, 0, SEEK_END);
        unsigned long len = (unsigned long)ftell(f);
        fseek(f, 0, SEEK_SET);

        char *buff = (char*)malloc(len + 1);
        if (fread(buff, 1, len, f) != len)
            return -1;
        fclose(f);
        
        source_range_t* sr = src_init_source(buff, len);
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
    validate_tl(ast);  
    code_gen(ast, &asm_print);
}

