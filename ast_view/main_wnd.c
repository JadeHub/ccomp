#include "pch.h"
#include "ast_view.h"

#define MAX_LOADSTRING 100

HWND hWndTree;
HWND hWndSource;
HWND hMainWnd;
HWND hWndOutput;
CHAR szWindowClass[MAX_LOADSTRING];            // the main window class name


SourceFile* theSourceFile = NULL;

ATOM                RegisterMainWindowClass(HINSTANCE hInstance);

BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

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


DWORD EditStreamCallback(DWORD_PTR dwCookie,
    LPBYTE lpBuff,
    LONG cb,
    PLONG pcb)
{
    asm_out_t** asm = ((asm_out_t**)dwCookie);

    if (*asm)
    {
        size_t len = strlen((*asm)->line);
        strcpy_s(lpBuff, cb, (*asm)->line);
        lpBuff[len] = '\r';
        lpBuff[len+1] = '\0';
        *pcb = (LONG)len+1;
        *asm = (*asm)->next;        
        return 0;
    }
    return -1;
}

void PopulateOutputWindow(HWND hWndOutput, SourceFile* sf)
{
    asm_out_t* asm = sf->asm;

    EDITSTREAM es = { 0 };

    es.pfnCallback = EditStreamCallback;
    es.dwCookie = (DWORD_PTR)&asm;


    CHARFORMAT2 fmt;
    memset(&fmt, 0, sizeof(CHARFORMAT2));
    fmt.cbSize = sizeof(CHARFORMAT2);
    fmt.dwMask = CFM_BACKCOLOR;
    fmt.crBackColor = 0x0000F800;

    

    if (SendMessage(hWndOutput, EM_STREAMIN, SF_TEXT, (LPARAM)&es) && es.dwError == 0)
    {
        
    }


    //SendMessage(hWndOutput, WM_SETTEXT, 0, (LPARAM)"hello");
}

void OpenSourceFile(LPCTSTR path)
{
    DestroySourceFile(theSourceFile);
    theSourceFile = LoadSourceFile(path);
    if (theSourceFile)
    {
        PopulateAstTreeView(theSourceFile);
        TextCtrl_PopulateSourceText(hWndSource, theSourceFile);

        PopulateOutputWindow(hWndOutput, theSourceFile);
    }
    SetWindowText(hMainWnd, path);
}

bool is_white_spce(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

void HighlightTextRange(AstTreeItem* ast)
{
     if (ast->tokens.start && ast->tokens.end)
     {
         const char* start = ast->tokens.start->loc;
         const char* end = ast->tokens.end->loc;

         while (is_white_spce(*start) && start < end)
             start++;
         while (is_white_spce(*(end-1)) && end > start)
             end--;

         TextCtrl_HighLightSource(hWndSource, (LONG)(start - theSourceFile->SourceRange.ptr),
                                    (LONG)(end - theSourceFile->SourceRange.ptr));
     }
     else
     {
         TextCtrl_HighLightSource(hWndSource, 0, 0);
     }
}

BOOL CreateMainWindow(HINSTANCE hInstance)
{
    hMainWnd = CreateWindow(szWindowClass, "Ast Viewer", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);
    if (!hMainWnd)
    {
        return FALSE;
    }

    if (!(hWndTree = CreateAstTreeView(hMainWnd)))
    {
        return FALSE;
    }

    if (!(hWndSource = CreateSrcTextCtrl(hMainWnd)))
    {
        return FALSE;
    }

    if(!(hWndOutput = CreateSrcTextCtrl(hMainWnd)))
    {
        return FALSE;
    }

    ShowWindow(hMainWnd, SW_NORMAL);
    UpdateWindow(hMainWnd);

    return TRUE;
}

ATOM RegisterMainWindowClass(HINSTANCE hInstance)
{
    LoadString(hInstance, IDC_ASTVIEW, szWindowClass, MAX_LOADSTRING);

    WNDCLASSEX wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ASTVIEW));
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCE(IDC_ASTVIEW);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassEx(&wcex);
}


void OnResize(HWND hwndParent)
{
    RECT rcClient;

    GetClientRect(hwndParent, &rcClient);

    int width = rcClient.right / 3;

    SetWindowPos(hWndSource, hwndParent,
        0, 
        0,
        width, 
        rcClient.bottom,
        SWP_SHOWWINDOW | SWP_NOZORDER);

    SetWindowPos(hWndTree, hwndParent,
        width,
        0,
        width,
        rcClient.bottom,
        SWP_SHOWWINDOW | SWP_NOZORDER);

    SetWindowPos(hWndOutput, hwndParent,
        width * 2,
        0,
        width,
        rcClient.bottom,
        SWP_SHOWWINDOW | SWP_NOZORDER);
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
        {
            char name[260];
            if (SelectFile(name))
            {
                OpenSourceFile(name);
            }
            break;
        }
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
    {
        NMHDR* nmhdr = (NMHDR*)lParam;

        if(nmhdr->code == TVN_SELCHANGED && nmhdr->hwndFrom == hWndTree)
        {
            NMTREEVIEW* info;
            info = (NMTREEVIEW*)nmhdr;
            AstTreeItem* ast = (AstTreeItem*)info->itemNew.lParam;
            HighlightTextRange(ast);
        }
        else if (nmhdr->hwndFrom == hWndSource && nmhdr->code == EN_SELCHANGE)
        {
            SELCHANGE* info = (SELCHANGE*)nmhdr;

            static bool selecting;

            if (selecting)
                return 0;

            selecting = true;

            token_t* tok = SrcFindTokenContaining(theSourceFile, info->chrg.cpMin);
            if (tok)
            {
                AstTree_SelectToken(tok);
            }

            selecting = false;
        }
        break;
    }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
