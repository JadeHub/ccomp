#include "parse_internal.h"
#include "diag.h"

#include <string.h>
#include <assert.h>

ast_declaration_t* parse_declarator(ast_type_spec_t* type_spec, uint32_t type_flags, decl_parse_context context);

ast_expression_t* parse_struct_union_init_expression()
{
	if (!expect_cur(tok_l_brace))
		return NULL;

	ast_expression_t* result = parse_alloc_expr();
	result->kind = expr_struct_init;
	next_tok();

	ast_expression_list_t* last_expr = NULL;
	while (!current_is(tok_r_brace))
	{
		//todo parse designator
		ast_expression_list_t* member_init_expr = (ast_expression_list_t*)malloc(sizeof(ast_expression_list_t));
		memset(member_init_expr, 0, sizeof(ast_expression_list_t));

		if (current_is(tok_l_brace))
		{
			//nested struct
			member_init_expr->expr = parse_struct_union_init_expression();
		}
		else
		{
			member_init_expr->expr = parse_constant_expression();
		}
		if (!member_init_expr->expr)
		{
			free(member_init_expr);
			ast_destroy_expression(result);
			return NULL;
		}

		if (result->data.struct_init.exprs == NULL)
			result->data.struct_init.exprs = member_init_expr;
		else
			last_expr->next = member_init_expr;
		last_expr = member_init_expr;


		if (current_is(tok_comma))
			next_tok();
	}
	next_tok();

	result->tokens.end = current();
	return result;
}

/*
<array_decl> ::= "[" [ <constant-exp> ] "]"

returns NULL if no array spec found
returns an expression of kind expr_null if empty array spec found '[]'
otherwise returns an expression representing the array size
*/
ast_expression_t* opt_parse_array_spec()
{
	ast_expression_t* result = NULL;
	if (current_is(tok_l_square_paren))
	{
		next_tok();

		if (current_is(tok_r_square_paren))
		{
			result = parse_alloc_expr();
			result->kind = expr_null;
		}
		else
		{
			result = parse_constant_expression();

		}
		expect_cur(tok_r_square_paren);
		next_tok();
	}
	return result;
}

/*
<function_param_list> ::= <function_param_decl> { "," <function_param_decl> }
<function_param_decl> ::= <declaration_specifiers> { "*" } <id> [ <array_decl> ]
*/
ast_func_params_t* parse_function_parameters()
{
	ast_func_params_t* params = (ast_func_params_t*)malloc(sizeof(ast_func_params_t));
	memset(params, 0, sizeof(ast_func_params_t));

	expect_cur(tok_l_paren);
	next_tok();

	params->first_param = params->last_param = NULL;
	params->param_count = 0;
	params->ellipse_param = false;
	while (!current_is(tok_eof))
	{
		if (current_is(tok_ellipse) || current_is(tok_r_paren))
			break;

		uint32_t type_flags = 0;
		ast_type_spec_t* type_spec = try_parse_type_spec(&type_flags);
		if (!type_spec)
			return parse_err(ERR_UNKNOWN_TYPE, "Expected parameter type, found %s", tok_kind_spelling(current()->kind));
		ast_declaration_t* decl = parse_declarator(type_spec, type_flags, dpc_param);
		if (!decl)
			return parse_err(ERR_UNKNOWN_TYPE, "Error parsing declaration");

		ast_func_param_decl_t* param = (ast_func_param_decl_t*)malloc(sizeof(ast_func_param_decl_t));
		memset(param, 0, sizeof(ast_func_param_decl_t));
		param->decl = decl;

		if (!params->first_param)
		{
			params->first_param = params->last_param = param;
		}
		else
		{
			param->prev = params->last_param;
			params->last_param->next = param;
			params->last_param = param;
		}
		params->param_count++;

		if (!current_is(tok_comma))
			break;
		next_tok();
	}

	// if single void param remove it
	if (params->param_count == 1 && params->first_param->decl->type_ref->spec->kind == type_void)
	{
		free(params->first_param);
		params->first_param = params->last_param = NULL;
		params->param_count = 0;
	}

	if (current_is(tok_ellipse))
	{
		next_tok();
		params->ellipse_param = true;
	}

	expect_cur(tok_r_paren);
	next_tok();
	return params;
}

ast_declaration_t* parse_declarator(ast_type_spec_t* type_spec, uint32_t type_flags, decl_parse_context context)
{
	parse_type_ref_result_t type_ref_parse = parse_type_ref(type_spec, type_flags);
	ast_type_ref_t* type_ref = type_ref_parse.type;

	ast_declaration_t* result = (ast_declaration_t*)malloc(sizeof(ast_declaration_t));
	memset(result, 0, sizeof(ast_declaration_t));
	result->tokens.start = current();
	result->tokens.end = current();
	result->type_ref = type_ref;

	result->kind = decl_type;

	if(ast_type_is_fn_ptr(type_ref->spec))
	{
		if (type_ref_parse.identifier)
		{
			//copy the name seen while parsing the function pointer type
			tok_spelling_cpy(type_ref_parse.identifier, result->name, MAX_LITERAL_NAME);
		}
	}	
	else if (current_is(tok_identifier))
	{
		tok_spelling_cpy(current(), result->name, MAX_LITERAL_NAME);
		next_tok();
	}

	if (strlen(result->name) && current_is(tok_l_paren))
	{
		//function
		result->kind = decl_func;
		ast_func_params_t* params = parse_function_parameters();
		result->type_ref->spec = ast_make_func_sig_type(result->type_ref->spec, params);
	}
	else
	{
		result->kind = decl_type;

		ast_expression_list_t* array_sz_list = NULL;
		ast_expression_t* array_sz_expr = opt_parse_array_spec();
		while (array_sz_expr)
		{
			ast_expression_list_t* array_sz = (ast_expression_list_t*)malloc(sizeof(ast_expression_list_t));
			memset(array_sz, 0, sizeof(ast_expression_list_t));
			array_sz->expr = array_sz_expr;

			if (result->array_dimensions == NULL)
				result->array_dimensions = array_sz; //first 
			else
				array_sz_list->next = array_sz;
			array_sz_list = array_sz;

			//array implies ptr type
			result->type_ref->spec = ast_make_ptr_type(result->type_ref->spec);

			array_sz_expr = opt_parse_array_spec();
		}

		if (context == dpc_struct)
		{
			//bit size is permitted
			if (current_is(tok_colon))
			{
				next_tok();
				result->data.var.bit_sz = parse_constant_expression();
			}
		}

		if (context == dpc_normal)
		{
			//initialiser is permitted
			if (current_is(tok_equal))
			{
				next_tok();

				if (ast_type_is_struct_union(type_ref->spec) && current_is(tok_l_brace))
				{
					result->data.var.init_expr = parse_struct_union_init_expression();
				}
				else
				{
					result->data.var.init_expr = parse_expression();
				}
			}
		}

		if (type_ref->flags & TF_SC_TYPEDEF)
		{
			//typedef
			if (strlen(result->name) == 0)
			{
				parse_err(ERR_SYNTAX, "typedef requires a name");
				ast_destroy_declaration(result);
				return NULL;
			}

			if (result->data.var.init_expr)
			{
				parse_err(ERR_SYNTAX, "typedef cannot be initialised");
				ast_destroy_declaration(result);
				return NULL;
			}			
			parse_register_alias_name(result->name);
		}
		else if (strlen(result->name))
		{
			//variable 
			result->kind = decl_var;
		}
		else
		{
			//unnamed param or type decl
		}
	}
	result->tokens.end = current();
	return result;
}

ast_decl_list_t try_parse_decl_list(decl_parse_context context)
{
	uint32_t type_flags = 0;
	ast_decl_list_t decl_list = { NULL, NULL };
	ast_type_spec_t* type_spec = try_parse_type_spec(&type_flags);
	if (!type_spec)
		goto _err_ret;

	while (!current_is(tok_eof))
	{		
		ast_declaration_t* decl = parse_declarator(type_spec, type_flags, context);
		if (!decl)
			goto _err_ret;

		if (!decl_list.first)
		{
			decl_list.first = decl_list.last = decl;
		}
		else
		{
			decl_list.last->next = decl;
			decl_list.last = decl;

			if (decl->kind == decl_type)
			{
				//any type declaration must be first in the list
				parse_err(ERR_SYNTAX, "variable or function declaration expected");
				goto _err_ret;
			}
		}

		if (!current_is(tok_comma))
			break;
		next_tok();
	}

	return decl_list;
_err_ret:
	ast_destroy_decl_list(decl_list);
	decl_list.first = decl_list.last = NULL;
	return decl_list;
	
}