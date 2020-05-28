#include "pch.h"
#include "ast_view.h"

HWND hWndSrc;

HWND CreateSrcTextCtrl(HWND hwndParent)
{
    RECT rcClient;

    GetClientRect(hwndParent, &rcClient);
    hWndSrc = CreateWindowEx(0, "RICHEDIT50W", TEXT("Type here"),
        ES_MULTILINE | WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP,
        rcClient.right / 2, 0,
        rcClient.right / 2, rcClient.bottom,
        hwndParent, NULL, hInst, NULL);

    SendMessage(hWndSrc, EM_SETREADONLY, 1, 0);
    return hWndSrc;
}

void PopulateSourceText(SourceFile* sf)
{
    SendMessage(hWndSrc, WM_SETTEXT, 0, (LPARAM)sf->SourceRange.ptr);
}