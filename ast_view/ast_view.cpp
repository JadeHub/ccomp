// ast_view.cpp : Defines the entry point for the application.
//

#include "pch.h"
#include "ast_view.h"

#include "diag.h"
#include "lexer.h"
#include "parse.h"

#define MAX_LOADSTRING 100

HWND CreateAstTreeView(HWND hwndParent);
HWND CreateSrcTextCtrl(HWND hParent);
void PopulateAstTreeView(SourceFile* sf);
void PopulateSourceText(SourceFile* sf);

// Global Variables:
HINSTANCE hInst;                                // current instance
WCHAR szTitle[MAX_LOADSTRING];                  // The title bar text
WCHAR szWindowClass[MAX_LOADSTRING];            // the main window class name
HWND hMainWnd;
SourceFile* theSourceFile = NULL;

ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

char* OpenFile(LPTSTR path)
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

void diag_err(token_t* tok, uint32_t err, const char* msg, void* data)
{
    MessageBox(hMainWnd, msg, "Error", MB_ICONERROR);
}

void DestroySourceFile(SourceFile* sf)
{
    if (!sf) return;

    ast_destory_translation_unit(sf->pAst);
    free(sf);
}

SourceFile* LoadSourceFile(LPTSTR path)
{
    source_range_t sr;
    sr.ptr = OpenFile(path);
    if (!sr.ptr)
        return NULL;
    sr.end = sr.ptr + strlen(sr.ptr);
    SourceFile* sf = (SourceFile*)malloc(sizeof(SourceFile));
    memset(sf, 0, sizeof(SourceFile));
    sf->SourceRange = sr;
    strcpy_s(sf->szFileName, PathFindFileName(path));

    diag_set_handler(diag_err, NULL);

    sf->pTokens = lex_source(&sr);
    sf->pAst = parse_translation_unit(sf->pTokens);

    return sf;
}

BOOL SelectFile(LPTSTR szFile)
{
    OPENFILENAME ofn;

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hMainWnd;
    ofn.lpstrFile = szFile;
    // Set lpstrFile[0] to '\0' so that GetOpenFileName does not 
    // use the contents of szFile to initialize itself.
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = 256;
    ofn.lpstrFilter = "All\0*.*\0C Source\0*.c\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    return GetOpenFileName(&ofn);
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // TODO: Place code here.

    // Initialize global strings
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_ASTVIEW, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_ASTVIEW));

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}

//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ASTVIEW));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_ASTVIEW);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

HWND hTree;
HWND hText;

void OnResize(HWND hwndParent)
{
    RECT rcClient;

    GetClientRect(hwndParent, &rcClient);

    SetWindowPos(hTree, hwndParent, 
        0, 0, 
        rcClient.right / 2, rcClient.bottom,
        SWP_SHOWWINDOW | SWP_NOZORDER);

    SetWindowPos(hText, hwndParent, 
        rcClient.right / 2, 0, 
        rcClient.right / 2, rcClient.bottom, 
        SWP_SHOWWINDOW | SWP_NOZORDER);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Store instance handle in our global variable

   InitCommonControls();

   LoadLibrary(TEXT("Msftedit.dll"));

   hMainWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);
   if (!hMainWnd)
   {
      return FALSE;
   }

   if (!(hTree = CreateAstTreeView(hMainWnd)))
   {
       return FALSE;
   }

   if (!(hText = CreateSrcTextCtrl(hMainWnd)))
   {
       return FALSE;
   }

   ShowWindow(hMainWnd, nCmdShow);
   UpdateWindow(hMainWnd);

   return TRUE;
}

#include <stdio.h>

void HighlightTextRange(AstTreeItem* ast)
{
    if (ast->tokens.start)
    {
        SendMessage(hText, EM_SETSEL,
            (LONG)(ast->tokens.start->loc - theSourceFile->SourceRange.ptr),
            (LONG)(ast->tokens.end->loc - theSourceFile->SourceRange.ptr));
    }
    else
    {
        SendMessage(hText, EM_SETSEL, 0, 0);
    }

    SendMessage(hText, WM_SETFOCUS, 0, 0);
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            // Parse the menu selections:
            switch (wmId)
            {
            case IDM_ABOUT:
                DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
                break;
            case IDM_EXIT:
                DestroyWindow(hWnd);
                break;
            case IDM_OPEN:
                
                char name[260];
                if (SelectFile(name))
                {                    
                    DestroySourceFile(theSourceFile);
                    theSourceFile = LoadSourceFile(name);
                    if (theSourceFile)
                    {
                        PopulateAstTreeView(theSourceFile);
                        PopulateSourceText(theSourceFile);
                    }
                }
                break;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);

            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: Add any drawing code that uses hdc here...
            EndPaint(hWnd, &ps);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    case WM_SIZE:
        OnResize(hWnd);
        break;
    case WM_NOTIFY:
        NMTREEVIEW* info;

        info = (NMTREEVIEW*)lParam;
        if (info->hdr.code == TVN_SELCHANGEDW &&
            info->hdr.hwndFrom == hTree)
        {
            AstTreeItem* ast = (AstTreeItem*) info->itemNew.lParam;
            HighlightTextRange(ast);
        }
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
