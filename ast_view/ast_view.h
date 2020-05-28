#pragma once

#include "resource.h"
#include "lexer.h"
#include "parse.h"

extern HINSTANCE hInst;

typedef struct
{
	char szFileName[256];
	source_range_t SourceRange;
	token_t* pTokens;
	ast_trans_unit_t* pAst;
	LPTSTR szErrorMsg;
	DWORD dwError;
}SourceFile;

typedef struct
{
	char name[128];
	
	token_range_t tokens;

}AstTreeItem;