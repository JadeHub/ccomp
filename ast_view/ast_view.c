// ast_view.cpp : Defines the entry point for the application.
//

#include "pch.h"
#include "ast_view.h"

#include <stdio.h>


HWND CreateAstTreeView(HWND hwndParent);
HWND CreateSrcTextCtrl(HWND hParent);

// Global Variables:
HINSTANCE hInst;                                // current instance



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

    return CreateMainWindow(hInst);
}


int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    RegisterMainWindowClass(hInstance);

    // Perform application initialization:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_ASTVIEW));

    OpenSourceFile("C:\\Users\\grayj\\code\\compiler\\test_code\\write_a_c_compiler\\stage_13\\valid\\complex.c");

    MSG msg;

    // Main message loop:
    while (GetMessage(&msg, NULL, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}

void asm_print(const char* line)
{
//    printf("%s\n", line);
}



