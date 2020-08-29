#include "pch.h"
#include "ast_view.h"

#include <stdio.h>

HWND hWndTree;
int iImgIdx_C;

void InsertBlockItem(HTREEITEM parent, ast_block_item_t* blk);
void InsertTypeSpec(HTREEITEM parent, ast_type_spec_t* type, const char* prefix);
void InsertTypeSpecRef(HTREEITEM parent, ast_type_ref_t* type, const char* prefix);

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
        sprintf_s(item->name, 128,"%sBinary Op: %s", prefix, ast_op_name(expr->data.binary_op.operation));
        tree_item = TreeInsert(parent, item);
        InsertExpression(tree_item, expr->data.binary_op.lhs, "lhs: ");
        InsertExpression(tree_item, expr->data.binary_op.rhs, "rhs: ");
        break;
    case expr_unary_op:
        sprintf_s(item->name, 128,"%sUnary Op: %s", prefix, ast_op_name(expr->data.unary_op.operation));
        tree_item = TreeInsert(parent, item);
        InsertExpression(tree_item, expr->data.unary_op.expression, "expr: ");
        break;
    case expr_postfix_op:
        sprintf_s(item->name, 128,"%sPostfix Op: %s", prefix, ast_op_name(expr->data.unary_op.operation));
        tree_item = TreeInsert(parent, item);
        InsertExpression(tree_item, expr->data.unary_op.expression, "expr: ");
        break;
    case expr_int_literal:
        sprintf_s(item->name, 128,"%sInt Literal %d", prefix, expr->data.int_literal.value);
        tree_item = TreeInsert(parent, item);
        break;
    case expr_assign:
        sprintf_s(item->name, 128,"%sAssignment", prefix);
        tree_item = TreeInsert(parent, item);
        InsertExpression(tree_item, expr->data.assignment.target, "target: ");
        InsertExpression(tree_item, expr->data.assignment.expr, "expr: ");
        break;
    case expr_identifier:
        sprintf_s(item->name, 128,"%sID: %s", prefix, expr->data.var_reference.name);
        tree_item = TreeInsert(parent, item);
        break;
    case expr_condition:
        sprintf_s(item->name, 128,"%sCondition", prefix);
        tree_item = TreeInsert(parent, item);
        InsertExpression(tree_item, expr->data.condition.cond, "cond: ");
        InsertExpression(tree_item, expr->data.condition.true_branch, "true: ");
        if(expr->data.condition.false_branch)
            InsertExpression(tree_item, expr->data.condition.false_branch, "false: ");
        break;
    case expr_func_call:
        sprintf_s(item->name, 128,"%sFunc Call: %s", prefix, expr->data.func_call.name);
        tree_item = TreeInsert(parent, item);
        InsertExpression(tree_item, expr->data.func_call.target, "target: ");
        sub_expr = expr->data.func_call.params;
        while (sub_expr)
        {
            InsertExpression(tree_item, sub_expr, "param ");
            sub_expr = sub_expr->next;
        }
        break;
    case expr_null:
        sprintf_s(item->name, 128,"%sNull Expr", prefix);
        tree_item = TreeInsert(parent, item);
        break;
    default:
        sprintf_s(item->name, 128,"%sUnknown expression kind %d", prefix, expr->kind);
        tree_item = TreeInsert(parent, item);
    }
}

void InsertStructDefinition(HTREEITEM parent, ast_type_spec_t* spec, const char* prefix)
{
    AstTreeItem* item = AllocTreeItem(spec->user_type_spec->tokens);
    sprintf_s(item->name, 128,"%s%s %s", prefix, 
        spec->user_type_spec->kind == user_type_struct ? "struct" : "union",
        spec->user_type_spec->name);
    HTREEITEM tree_item = TreeInsert(parent, item);

    ast_struct_member_t* member = spec->user_type_spec->struct_members;

    while (member)
    {
        AstTreeItem* item = AllocTreeItem(member->tokens);
        if(member->bit_size)
            sprintf_s(item->name, 128,"member: %s bits: %d", member->name, member->bit_size);
        else
            sprintf_s(item->name, 128,"member: %s", member->name);
        HTREEITEM mh = TreeInsert(tree_item, item);
        InsertTypeSpecRef(mh, member->type_ref, "");
                
        member = member->next;
    }
}

void InsertEnumDefinition(HTREEITEM parent, ast_type_spec_t* spec, const char* prefix)
{
    AstTreeItem* item = AllocTreeItem(spec->user_type_spec->tokens);
    sprintf_s(item->name, 128, "%senum %s", prefix,
        spec->user_type_spec->name);
    HTREEITEM tree_item = TreeInsert(parent, item);

    ast_enum_member_t* member = spec->user_type_spec->enum_members;

    while (member)
    {
        AstTreeItem* item = AllocTreeItem(member->tokens);
        sprintf_s(item->name, 128, "%s", member->name);
        HTREEITEM mh = TreeInsert(tree_item, item);
        if (member->const_value)
        {
            InsertExpression(mh, member->const_value, "Val ");
        }
        
        member = member->next;
    }
}

void InsertTypeSpecRef(HTREEITEM parent, ast_type_ref_t* type, const char* prefix)
{
    if (type->spec->kind == type_user)
    {
        if (type->spec->user_type_spec->kind == user_type_enum)
            InsertEnumDefinition(parent, type->spec, prefix);
        else
            InsertStructDefinition(parent, type->spec, prefix);
    }
    else
    {
        AstTreeItem* item = AllocTreeItem(type->tokens);
        sprintf_s(item->name, 128,"%s%s", prefix, ast_type_ref_name(type));
        HTREEITEM tree_item = TreeInsert(parent, item);
    }
}

void InsertTypeSpec(HTREEITEM parent, ast_type_spec_t* type, const char* prefix)
{
    if (type->kind == type_user)
    {
        if(type->user_type_spec->kind == user_type_enum)
            InsertEnumDefinition(parent, type, prefix);
        else
            InsertStructDefinition(parent, type, prefix);
    }
    else
    {
        /*AstTreeItem* item = AllocTreeItem(type->tokens);
        sprintf_s(item->name, 128,"%s%s", prefix, ast_type_name(type));
        HTREEITEM tree_item = TreeInsert(parent, item);*/
    }
}

void InsertVariableDefinition(HTREEITEM parent, ast_declaration_t* var, const char* prefix)
{
    AstTreeItem* item = AllocTreeItem(var->tokens);
    sprintf_s(item->name, 128,"%sVar \'%s\' Type \'%s\'", prefix, var->data.var.name, ast_type_ref_name(var->data.var.type_ref));
    HTREEITEM tree_item = TreeInsert(parent, item);
    if (var->data.var.expr)
    {
        InsertExpression(tree_item, var->data.var.expr, "= ");
    }
}

HTREEITEM InsertFunctionDeclaration(HTREEITEM parent, ast_declaration_t* func, const char* prefix)
{
    AstTreeItem* item = AllocTreeItem(func->tokens);
    sprintf_s(item->name, 128,"%sFunction: %s", prefix, func->data.func.name);
    HTREEITEM tree_item = TreeInsert(parent, item);

    InsertTypeSpecRef(tree_item, func->data.func.return_type_ref, "Return Type: ");

    ast_func_param_decl_t* param = func->data.func.first_param;
    while (param)
    {
        AstTreeItem* item = AllocTreeItem(param->decl->tokens);
        sprintf_s(item->name, 128,"param: %s", param->decl->data.var.name);
        HTREEITEM hp = TreeInsert(tree_item, item);
        InsertTypeSpecRef(hp, param->decl->data.var.type_ref, "");
        param = param->next;
    }

    ast_block_item_t* blk = func->data.func.blocks;
    while (blk)
    {
        InsertBlockItem(tree_item, blk);
        blk = blk->next;
    }

    return tree_item;
}

void InsertDeclaration(HTREEITEM parent, ast_declaration_t* decl, const char* prefix)
{
    switch (decl->kind)
    {
    case decl_var:
        InsertVariableDefinition(parent, decl, prefix);
        break;
    case decl_func:
        InsertFunctionDeclaration(parent, decl, prefix);
        break;
    case decl_type:
        InsertTypeSpec(parent, decl->data.type, prefix);
        break;
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
        InsertExpression(parent, smnt->data.expr, "expr: ");
        break;
    case smnt_return:
        sprintf_s(item->name, 128,"%sReturn Statement", prefix);
        tree_item = TreeInsert(parent, item);
        if (smnt->data.expr)
        {
            InsertExpression(tree_item, smnt->data.expr, "expr: ");
        }
        break;
    case smnt_if:
        sprintf_s(item->name, 128,"%sIf Statement", prefix);
        tree_item = TreeInsert(parent, item);
        InsertExpression(tree_item, smnt->data.if_smnt.condition, "cond: ");
        InsertStatement(tree_item, smnt->data.if_smnt.true_branch, "true: ");
        if(smnt->data.if_smnt.false_branch)
            InsertStatement(tree_item, smnt->data.if_smnt.false_branch, "false: ");
        break;
    case smnt_compound:
        sprintf_s(item->name, 128,"%sCompound", prefix);
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
        sprintf_s(item->name, 128,"%sfor", prefix);
        tree_item = TreeInsert(parent, item);

        if(smnt->data.for_smnt.init)
            InsertExpression(tree_item, smnt->data.for_smnt.init, "init ");
        if (smnt->data.for_smnt.init_decl)
            InsertDeclaration(tree_item, smnt->data.for_smnt.init_decl, "init ");

        if (smnt->data.for_smnt.condition)
            InsertExpression(tree_item, smnt->data.for_smnt.condition, "cond ");

        if (smnt->data.for_smnt.post)
            InsertExpression(tree_item, smnt->data.for_smnt.post, "post ");

        InsertStatement(tree_item, smnt->data.for_smnt.statement, "body ");
        break;    
    case smnt_while:
        sprintf_s(item->name, 128,"%swhile", prefix);
        tree_item = TreeInsert(parent, item);
        InsertExpression(tree_item, smnt->data.while_smnt.condition, "cond ");
        InsertStatement(tree_item, smnt->data.while_smnt.statement, "body ");
        break;
    case smnt_do:
        sprintf_s(item->name, 128,"%sdo..while", prefix);
        tree_item = TreeInsert(parent, item);
        InsertStatement(tree_item, smnt->data.while_smnt.statement, "body ");
        InsertExpression(tree_item, smnt->data.while_smnt.condition, "cond ");        
        break;
    case smnt_break:
        sprintf_s(item->name, 128,"%sbreak", prefix);
        tree_item = TreeInsert(parent, item);
        break;
    case smnt_continue:
        sprintf_s(item->name, 128,"%scontinue", prefix);
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
    else if (blk->kind == blk_decl)
    {
        InsertDeclaration(parent, blk->decl, "");
    }
}

void InsertTranslationUnit(HTREEITEM parent, LPCTSTR file_name, valid_trans_unit_t* tl)
{
    AstTreeItem* item = AllocTreeItem(tl->ast->tokens);
    sprintf_s(item->name, 128,"TranslationUnit: %s", file_name);
    HTREEITEM tree_item = TreeInsert(parent, item);

    tl_decl_t* type_decl = tl->type_decls;
    while (type_decl)
    {
        InsertDeclaration(tree_item, type_decl->decl, "");
        type_decl = type_decl->next;
    }

    tl_decl_t* var_decl = tl->var_decls;
    while (var_decl)
    {
        InsertDeclaration(tree_item, var_decl->decl, "Global ");
        var_decl = var_decl->next;
    }

    tl_decl_t* fn_decl = tl->fn_decls;
    while (fn_decl)
    {
        InsertDeclaration(tree_item, fn_decl->decl, "");
        fn_decl = fn_decl->next;
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
    InsertTranslationUnit(TVI_ROOT, (LPCTSTR)sf->szFileName, sf->pTransUnit);
    TreeView_Expand(hWndTree, TreeView_GetRoot(hWndTree), TVE_EXPAND);
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

typedef struct
{
    HTREEITEM item;
    token_range_t* range;
}tok_find_result_t;

static token_range_t* _best_match(token_range_t* r1, token_range_t* r2)
{
    if (!r1)
        return r2;
    if (!r2)
        return r1;
    return tok_range_len(r1) < tok_range_len(r2) ? r1 : r2;
}

void FindToken(HTREEITEM item, token_t* tok, tok_find_result_t* bestMatch)
{
    TVITEM tvi;

    tvi.mask = TVIF_PARAM;
    tvi.hItem = item;    
    if (!TreeView_GetItem(hWndTree, &tvi))
        return;

    AstTreeItem* data = (AstTreeItem*)tvi.lParam;

    if (tok_is_in_range(tok, &data->tokens))
    {
        if (!bestMatch->range || tok_range_len(&data->tokens) < tok_range_len(bestMatch->range))
        {
            bestMatch->item = item;
            bestMatch->range = &data->tokens;
        }
    }

    HTREEITEM child = TreeView_GetChild(hWndTree, item);
    while (child)
    {
        FindToken(child, tok, bestMatch);
        child = TreeView_GetNextSibling(hWndTree, child);
    }
}

void AstTree_SelectToken(token_t* tok)
{
    HTREEITEM root = TreeView_GetRoot(hWndTree);
    
    tok_find_result_t bestMatch = { 0, 0 };

    FindToken(root, tok, &bestMatch);
    if (bestMatch.item)
    {
        TreeView_SelectItem(hWndTree, bestMatch.item);

        SendMessage(hWndTree, WM_SETFOCUS, 0, 0);
    }
}

HWND CreateAstTreeView(HWND hwndParent)
{
    hWndTree = CreateWindowEx(0,
        WC_TREEVIEW,
        TEXT("Tree View"),
        WS_VISIBLE | WS_CHILD | WS_BORDER | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS,
        0, 0, 10, 10,
        hwndParent,
        NULL,
        hInst,
        NULL);

    InitTreeViewImageLists(hWndTree);

    return hWndTree;
}