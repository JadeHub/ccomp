#pragma once

#include "resource.h"

extern HINSTANCE hInst;
extern HWND hMainWnd;

typedef struct asm_out
{
	const char* line;
	struct asm_out* next;
}asm_out_t;

typedef struct err_out
{
	char* msg;
	uint32_t code;
	struct err_out* next;
}err_out_t;

typedef struct
{
	char szFileName[256];
	source_range_t SourceRange;
	token_t* pTokens;
	ast_trans_unit_t* pAst;
	valid_trans_unit_t* pTransUnit;

	err_out_t* errors;
	asm_out_t* asm;
}SourceFile;

typedef struct
{
	char name[128];	
	token_range_t tokens;
}AstTreeItem;

// main_wnd.c
BOOL CreateMainWindow(HINSTANCE hInstance);
ATOM RegisterMainWindowClass(hInstance);

//ast_tree_ctrl.c
HWND CreateAstTreeView(HWND hwndParent);
void PopulateAstTreeView(SourceFile* sf);
void AstTree_SelectToken(token_t* tok);

//source_text_ctrl.c
HWND CreateSrcTextCtrl(HWND hwndParent);
void TextCtrl_PopulateSourceText(HWND  wnd, SourceFile* sf);
void TextCtrl_HighLightSource(HWND  wnd, LONG start, LONG end);

//source_file.c
SourceFile* LoadSourceFile(LPCTSTR path);
void DestroySourceFile(SourceFile* sf);
token_t* SrcFindTokenContaining(SourceFile* sf, long loc);

BOOL SelectFile(LPTSTR szFile);
void OpenSourceFile(LPCTSTR path);