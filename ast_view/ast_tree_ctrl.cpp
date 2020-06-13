#include "pch.h"
#include "ast_view.h"

#include <stdio.h>

HWND hWndTree;
int iImgIdx_C;

void InsertBlockItem(HTREEITEM parent, ast_block_item_t* blk);

AstTreeItem* AllocTreeItem(token_range_t tok_range)
{
    AstTreeItem* result = (AstTreeItem*)malloc(sizeof(AstTreeItem));
    memset(result, 0, sizeof(AstTreeItem));
    result->tokens = tok_range;
    
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
    AstTreeItem* item = AllocTreeItem(expr->tokens);

    HTREEITEM tree_item;
    ast_expression_t* sub_expr;
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
    case expr_postfix_op:
        sprintf_s(item->name, "%sPostfix Op: %s", prefix, ast_op_name(expr->data.unary_op.operation));
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
    case expr_func_call:
        sprintf_s(item->name, "%sFunc Call: %s", prefix, expr->data.func_call.name);
        tree_item = TreeInsert(parent, item);
        sub_expr = expr->data.func_call.params;
        while (sub_expr)
        {
            InsertExpression(tree_item, sub_expr, "param ");
            sub_expr = sub_expr->next;
        }
        break;
    case expr_null:
        sprintf_s(item->name, "%sNull Expr", prefix);
        tree_item = TreeInsert(parent, item);
        break;
    default:
        sprintf_s(item->name, "%sUnknown expression kind %d", prefix, expr->kind);
        tree_item = TreeInsert(parent, item);
    }
}

void InsertVariableDefinition(HTREEITEM parent, ast_var_decl_t* var, const char* prefix)
{
    AstTreeItem* item = AllocTreeItem(var->tokens);
    sprintf_s(item->name, "%sVar Decl: %s", prefix, var->name);
    HTREEITEM tree_item = TreeInsert(parent, item);
    if (var->expr)
    {
        InsertExpression(tree_item, var->expr, "= expr: ");
    }
}

void InsertStatement(HTREEITEM parent, ast_statement_t* smnt, const char* prefix)
{
    AstTreeItem* item = AllocTreeItem(smnt->tokens);
    HTREEITEM tree_item;
    ast_block_item_t* block;

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

        block = smnt->data.compound.blocks;
        while (block)
        {
            InsertBlockItem(tree_item, block);
            block = block->next;
        }
        break;
    case smnt_for:
    case smnt_for_decl:
        sprintf_s(item->name, "%sfor", prefix);
        tree_item = TreeInsert(parent, item);

        if(smnt->data.for_smnt.init)
            InsertExpression(tree_item, smnt->data.for_smnt.init, "init ");
        if (smnt->data.for_smnt.init_decl)
            InsertVariableDefinition(tree_item, smnt->data.for_smnt.init_decl, "init ");

        if (smnt->data.for_smnt.condition)
            InsertExpression(tree_item, smnt->data.for_smnt.condition, "cond ");

        if (smnt->data.for_smnt.post)
            InsertExpression(tree_item, smnt->data.for_smnt.post, "post ");

        InsertStatement(tree_item, smnt->data.for_smnt.statement, "body ");
        break;    
    case smnt_while:
        sprintf_s(item->name, "%swhile", prefix);
        tree_item = TreeInsert(parent, item);
        InsertExpression(tree_item, smnt->data.while_smnt.condition, "cond ");
        InsertStatement(tree_item, smnt->data.while_smnt.statement, "body ");
        break;
    case smnt_do:
        sprintf_s(item->name, "%sdo..while", prefix);
        tree_item = TreeInsert(parent, item);
        InsertStatement(tree_item, smnt->data.while_smnt.statement, "body ");
        InsertExpression(tree_item, smnt->data.while_smnt.condition, "cond ");        
        break;
    case smnt_break:
        sprintf_s(item->name, "%sbreak", prefix);
        tree_item = TreeInsert(parent, item);
        break;
    case smnt_continue:
        sprintf_s(item->name, "%scontinue", prefix);
        tree_item = TreeInsert(parent, item);
        break;
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
        InsertVariableDefinition(parent, blk->var_decl, "");
    }
}

void InsertFunction(HTREEITEM parent, ast_function_decl_t* func)
{
    AstTreeItem* item = AllocTreeItem(func->tokens);
    sprintf_s(item->name, "Function: %s", func->name);
    HTREEITEM tree_item = TreeInsert(parent, item);

    ast_function_param_t* param = func->params;
    while (param)
    {
        AstTreeItem* item = AllocTreeItem(param->tokens);
        sprintf_s(item->name, "Parameter: %s", param->name);
        TreeInsert(tree_item, item);
        param = param->next;
    }

    ast_block_item_t* blk = func->blocks;

    while (blk)
    {
        InsertBlockItem(tree_item, blk);
        blk = blk->next;
    }
}

void InsertTranslationUnit(HTREEITEM parent, LPCTSTR file_name, ast_trans_unit_t* tl)
{
    AstTreeItem* item = AllocTreeItem(tl->tokens);
    sprintf_s(item->name, "TranslationUnit: %s", file_name);
    HTREEITEM tree_item = TreeInsert(parent, item);

    ast_function_decl_t* fn = tl->functions;
    while (fn)
    {
        InsertFunction(tree_item, fn);
        fn = fn->next;
    }
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