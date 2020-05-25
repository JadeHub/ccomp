
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <libcomp/include/token.h>
#include <libcomp/include/lexer.h>
#include <libcomp/include/parse.h>
#include <libcomp/include/code_gen.h>

//static const char* _src = "int main() \r\n{return 2;}";

static const char* _src = "int main() \r\n{return 1 << 2;}";

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

void diag_err(token_t* tok, uint32_t err, const char* msg)
{
    printf("%s\n", msg);
    exit(1);
}

int main(int argc, char* argv[])
{
    lex_init();

    int r = (-12) / 5;

    source_range_t sr;
    sr.ptr = _src;
    sr.end = sr.ptr + strlen(_src);
    if (argc == 2)
    {
        FILE* f = fopen(argv[1], "r");
        if (!f)
            return 1;

        fseek(f, 0, SEEK_END);
        unsigned long len = (unsigned long)ftell(f);

        fseek(f, 0, SEEK_SET);

        sr.ptr = (const char*)malloc(len+1);
        memset(sr.ptr, 0, len + 1);
        if (fread(sr.ptr, 1, len, f) != len)
            return -1;
        fclose(f);
        sr.end = sr.ptr + len;
    }

    token_t* toks = lex_source(&sr);
   // print_tokens(toks);
    ast_trans_unit_t* ast = parse_translation_unit(toks, &diag_err);
   // ast_print(ast);
    code_gen(ast, &asm_print);
}

