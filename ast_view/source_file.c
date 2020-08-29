#include "pch.h"

#include "ast_view.h"


void DestroySourceFile(SourceFile* sf)
{
    if (!sf) return;

    ast_destory_translation_unit(sf->pAst);
    free(sf);
}


static char* _read_source_file(LPCTSTR path)
{
    HANDLE hFile = CreateFile(path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL);

    if (hFile == INVALID_HANDLE_VALUE)
        return NULL;

    DWORD dwSize = GetFileSize(hFile, NULL);
    char* buff = (char*)malloc(dwSize + 1);

    if (!ReadFile(hFile, buff, dwSize, &dwSize, NULL))
    {
        free(buff);
        CloseHandle(hFile);
        return NULL;
    }
    buff[dwSize] = '\0';
    CloseHandle(hFile);
    return buff;
}

static void _error_handler(token_t* tok, uint32_t code, const char* msg, void* data)
{
    SourceFile* sf = (SourceFile*)data;

    err_out_t* err = (err_out_t*)malloc(sizeof(err_out_t));
    memset(err, 0, sizeof(err_out_t));
    err->code = code;
    err->msg = (char*)malloc(strlen(msg) + 1);
    strcpy_s(err->msg, strlen(msg) + 1, msg);
    err->next = sf->errors;
    sf->errors = err;
}

static void _asm_handler(const char* line, void* data)
{
    SourceFile* sf = (SourceFile*)data;
    size_t len = strlen(line);
    
    if (len == 0) return;
    
    asm_out_t* asm = (asm_out_t*)malloc(sizeof(asm_out_t));
    memset(asm, 0, sizeof(asm_out_t));
    asm->line = (const char*)malloc(len + 1);
    strcpy_s(asm->line, len + 1, line);

    if (!sf->asm)
    {
        sf->asm = asm;
        return;
    }

    asm_out_t* last = sf->asm;

    while (last->next)
        last = last->next;
    last->next = asm;
}

token_t* SrcFindTokenContaining(SourceFile* sf, long loc)
{
    token_t* tok = sf->pTokens;
    const char* start = sf->SourceRange.ptr;
    while (tok)
    {
        if (loc >= (tok->loc - start) && loc <= (tok->loc - start + tok->len))
            return tok;
        tok = tok->next;
    }
    return NULL;
}

SourceFile* LoadSourceFile(LPCTSTR path)
{
    source_range_t sr;
    sr.ptr = _read_source_file(path);
    if (!sr.ptr)
        return NULL;
    sr.end = sr.ptr + strlen(sr.ptr);

    SourceFile* sf = (SourceFile*)malloc(sizeof(SourceFile));
    memset(sf, 0, sizeof(SourceFile));
    sf->SourceRange = sr;
    strcpy_s(sf->szFileName, 256, PathFindFileName(path));
    
    diag_set_handler(_error_handler, sf);

    sf->pTokens = lex_source(&sr);
    sf->pAst = parse_translation_unit(sf->pTokens);
    if (sf->pAst)
        sf->pTransUnit = sem_analyse(sf->pAst);

    if (sf->pTransUnit)
    {
        code_gen(sf->pTransUnit, _asm_handler, sf);
    }

    diag_set_handler(NULL, NULL);

    return sf;
}
