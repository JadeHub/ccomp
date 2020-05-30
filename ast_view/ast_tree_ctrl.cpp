#include "pch.h"
#include "ast_view.h"

#include <stdio.h>

HWND hWndTree;
int iImgIdx_C;

void InsertBlockItem(HTREEITEM parent, ast_block_item_t* blk);

AstTreeItem* AllocTreeItem()
{
    AstTreeItem* result = (AstTreeItem*)malloc(sizeof(AstTreeItem));
    memset(result, 0, sizeof(AstTreeItem));

    return result;
}

HTREEITEM TreeInsert(HTREEITEM hParent, AstTreeItem* item)
{
    TVITEM tvi;
    TVINSERTSTRUCT tvins;

    tvi.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE;
    tvi.pszText = (LPTSTR)item->name;
    tvi.cchTextMax = (int)strlen(item->name);
    tvi.lParam = (LPARAM)item;
    tvi.iImage = iImgIdx_C;

    tvins.item = tvi;
    tvins.hParent = hParent;
    tvins.hInsertAfter = TVI_LAST;

    HTREEITEM tree_item = (HTREEITEM)SendMessage(hWndTree, TVM_INSERTITEM,
        0, (LPARAM)&tvins);

    return tree_item;
}

void InsertExpression(HTREEITEM parent, ast_expression_t* expr, const char* prefix)
{
    AstTreeItem* item = AllocTreeItem();
    item->tokens = expr->tokens;

    HTREEITEM tree_item;
    switch (expr->kind)
    {
    case expr_binary_op:
        sprintf_s(item->name, "%sBinary Op: %s", prefix, ast_op_name(expr->data.binary_op.operation));
        tree_item = TreeInsert(parent, item);
        InsertExpression(tree_item, expr->data.binary_op.lhs, "lhs: ");
        InsertExpression(tree_item, expr->data.binary_op.rhs, "rhs: ");
        break;
    case expr_unary_op:
        sprintf_s(item->name, "%sUnary Op: %s", prefix, ast_op_name(expr->data.unary_op.operation));
        tree_item = TreeInsert(parent, item);
        InsertExpression(tree_item, expr->data.unary_op.expression, "expr: ");
        break;
    case expr_int_literal:
        sprintf_s(item->name, "%sInt Literal: %d", prefix, expr->data.const_val);
        tree_item = TreeInsert(parent, item);
        break;
    case expr_assign:
        sprintf_s(item->name, "%sAssignment to %s", prefix, expr->data.assignment.name);
        tree_item = TreeInsert(parent, item);
        InsertExpression(tree_item, expr->data.assignment.expr, "expr: ");
        break;
    case expr_var_ref:
        sprintf_s(item->name, "%sVar Ref: %s", prefix, expr->data.var_reference.name);
        tree_item = TreeInsert(parent, item);
        break;
    case expr_condition:
        sprintf_s(item->name, "%sCondition", prefix);
        tree_item = TreeInsert(parent, item);
        InsertExpression(tree_item, expr->data.condition.cond, "cond: ");
        InsertExpression(tree_item, expr->data.condition.true_branch, "true: ");
        if(expr->data.condition.false_branch)
            InsertExpression(tree_item, expr->data.condition.false_branch, "false: ");
        break;
    default:
        sprintf_s(item->name, "%sUnknown expression kind %d", prefix, expr->kind);
        tree_item = TreeInsert(parent, item);
    }
}

void InsertStatement(HTREEITEM parent, ast_statement_t* smnt, const char* prefix)
{
    AstTreeItem* item = AllocTreeItem();
    item->tokens = smnt->tokens;
    HTREEITEM tree_item;

    item->tokens = smnt->tokens;
    switch (smnt->kind)
    {
    case smnt_expr:
        sprintf_s(item->name, "%sExpr Statement", prefix);
        tree_item = TreeInsert(parent, item);
        InsertExpression(tree_item, smnt->data.expr, "expr: ");
        break;
    case smnt_return:
        sprintf_s(item->name, "%sReturn Statement", prefix);
        tree_item = TreeInsert(parent, item);
        if (smnt->data.expr)
        {
            InsertExpression(tree_item, smnt->data.expr, "expr: ");
        }
        break;
    case smnt_if:
        sprintf_s(item->name, "%sIf Statement", prefix);
        tree_item = TreeInsert(parent, item);
        InsertExpression(tree_item, smnt->data.if_smnt.condition, "cond: ");
        InsertStatement(tree_item, smnt->data.if_smnt.true_branch, "true: ");
        if(smnt->data.if_smnt.false_branch)
            InsertStatement(tree_item, smnt->data.if_smnt.false_branch, "false: ");
        break;
    case smnt_compound:

        sprintf_s(item->name, "%sCompound", prefix);
        tree_item = TreeInsert(parent, item);

        ast_block_item_t* block = smnt->data.compound.blocks;
        while (block)
        {
            InsertBlockItem(tree_item, block);
            block = block->next;
        }

        break;
    }
}

void InsertVarDefItem(HTREEITEM parent, ast_var_decl_t* var)
{
    AstTreeItem* item = AllocTreeItem();
    item->tokens = var->tokens;
    sprintf_s(item->name, "Var Decl: %s", var->decl_name);
    HTREEITEM tree_item = TreeInsert(parent, item);
    if (var->expr)
    {
        InsertExpression(tree_item, var->expr, "= expr: ");
    }
}

void InsertBlockItem(HTREEITEM parent, ast_block_item_t* blk)
{
    if (blk->kind == blk_smnt)
    {
        InsertStatement(parent, blk->smnt, "");
    }
    else if (blk->kind == blk_var_def)
    {
        InsertVarDefItem(parent, blk->var_decl);
    }

    //item->tokens = func->tokens;
}

void InsertFunction(HTREEITEM parent, ast_function_decl_t* func)
{
    AstTreeItem* item = AllocTreeItem();
    item->tokens = func->tokens;
    sprintf_s(item->name, "Function: %s", func->name);
    HTREEITEM tree_item = TreeInsert(parent, item);

    ast_block_item_t* blk = func->blocks;

    while (blk)
    {
        InsertBlockItem(tree_item, blk);
        blk = blk->next;
    }
}

void InsertTranslationUnit(HTREEITEM parent, LPCTSTR file_name, ast_trans_unit_t* tl)
{
    AstTreeItem* item = AllocTreeItem();
    item->tokens = tl->tokens;
    sprintf_s(item->name, "TranslationUnit: %s", file_name);
    HTREEITEM tree_item = TreeInsert(parent, item);

    if (tl->function)
        InsertFunction(tree_item, tl->function);

}

void DeleteItem(HTREEITEM item)
{
    TVITEM data;
    data.hItem = item;
    data.mask = TVIF_PARAM;
    if (TreeView_GetItem(hWndTree, &data))
    {
        AstTreeItem* ast = (AstTreeItem*)data.lParam;
        free(ast);
    }
    TreeView_DeleteItem(hWndTree, item);
}

void DeleteChildren(HTREEITEM item)
{
    HTREEITEM child = TreeView_GetChild(hWndTree, item);
    while (child != NULL)
    {
        DeleteChildren(child);
        HTREEITEM next = TreeView_GetNextItem(hWndTree, child, TVGN_NEXT);
        DeleteItem(child);
        child = next;
    }
}

void PopulateAstTreeView(SourceFile* sf)
{    
    HTREEITEM root = TreeView_GetRoot(hWndTree);

    DeleteChildren(root);
    DeleteItem(root);
    InsertTranslationUnit(TVI_ROOT, (LPCTSTR)sf->szFileName, sf->pAst);
}

BOOL InitTreeViewImageLists(HWND hwndTV)
{
    HIMAGELIST himl;  // handle to image list 
    HBITMAP hbmp;     // handle to bitmap 

    // Create the image list. 
    if ((himl = ImageList_Create(16,
        16,
        FALSE,
        1, 0)) == NULL)
        return FALSE;

    // Add the open file, closed file, and document bitmaps. 
    hbmp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_C_IMG));
    iImgIdx_C = ImageList_Add(himl, hbmp, (HBITMAP)NULL);
    DeleteObject(hbmp);

    // Fail if not all of the images were added. 
    if (ImageList_GetImageCount(himl) < 1)
        return FALSE;

    // Associate the image list with the tree-view control. 
    TreeView_SetImageList(hwndTV, himl, TVSIL_NORMAL);

    return TRUE;
}

HWND CreateAstTreeView(HWND hwndParent)
{
    RECT rcClient;
    GetClientRect(hwndParent, &rcClient);
    hWndTree = CreateWindowEx(0,
        WC_TREEVIEW,
        TEXT("Tree View"),
        WS_VISIBLE | WS_CHILD | WS_BORDER | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS,
        0,
        0,
        rcClient.right/2,
        rcClient.bottom,
        hwndParent,
        NULL,
        hInst,
        NULL);

    InitTreeViewImageLists(hWndTree);

    return hWndTree;
}