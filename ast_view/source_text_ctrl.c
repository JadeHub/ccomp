#include "pch.h"
#include "ast_view.h"

HWND hWndSrc;

HWND CreateSrcTextCtrl(HWND hwndParent)
{
    hWndSrc = CreateWindowEx(0, "RICHEDIT50W", "",
        WS_VSCROLL | ES_MULTILINE | WS_VISIBLE | WS_CHILD | WS_BORDER | WS_TABSTOP,
        0,0,10,10,
        hwndParent, NULL, hInst, NULL);

    SendMessage(hWndSrc, EM_SETREADONLY, 1, 0);
    SendMessage(hWndSrc, EM_SETEVENTMASK, 0, ENM_SELCHANGE);
    return hWndSrc;
}

void TextCtrl_PopulateSourceText(HWND wnd, SourceFile* sf)
{
    SendMessage(wnd, WM_SETTEXT, 0, (LPARAM)sf->SourceRange.ptr);
}

void TextCtrl_HighLightSource(HWND wnd, LONG start, LONG end)
{
    SendMessage(wnd, EM_SETSEL, start, end);
    SendMessage(wnd, WM_SETFOCUS, 0, 0);
}
