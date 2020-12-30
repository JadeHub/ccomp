#include "parse_internal.h"
#include "diag.h"

#include <string.h>

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
void parse_function_parameters(ast_function_decl_t* func)
{
	while (1)
	{
		bool found_semi;

		ast_declaration_t* decl = try_parse_declaration_opt_semi(&found_semi);

		if (!decl)
			break;

		ast_func_param_decl_t* param = (ast_func_param_decl_t*)malloc(sizeof(ast_func_param_decl_t));
		memset(param, 0, sizeof(ast_func_param_decl_t));
		param->decl = decl;

		if (!func->first_param)
		{
			func->first_param = func->last_param = param;
		}
		else
		{
			param->prev = func->last_param;
			func->last_param->next = param;
			func->last_param = param;
		}
		func->param_count++;

		if (!current_is(tok_comma))
			break;
		next_tok();
	}

	// if single void param remove it
	if (func->param_count == 1 && func->first_param->decl->type_ref->spec->kind == type_void)
	{
		free(func->first_param);
		func->first_param = func->last_param = NULL;
		func->param_count = 0;
	}

	if (current_is(tok_ellipse))
	{
		next_tok();
		func->ellipse_param = true;
	}
}


/*
<declaration> :: = <var_declaration> ";"
				| <declaration_specifiers> ";"
				| <function_declaration> ";"

<var_declaration> ::= <declaration_specifiers> <id> [= <exp>]
<function_declaration> ::= <declaration_specifiers> <id> "(" [ <function_params ] ")"
<declaration_specifiers> ::= { ( <type_specifier> | <type_qualifier> | <storage_class_specifiers> ) }
*/
ast_declaration_t* try_parse_declaration_opt_semi(bool* found_semi)
{
	token_t* start = current();

	ast_type_ref_t* type_ref = try_parse_type_ref();
	if (!type_ref)
		return NULL;

	ast_declaration_t* result = (ast_declaration_t*)malloc(sizeof(ast_declaration_t));
	memset(result, 0, sizeof(ast_declaration_t));
	result->tokens.start = start;
	result->tokens.end = current();
	result->type_ref = type_ref;

	result->kind = decl_type;


	if (current_is(tok_identifier))
	{
		tok_spelling_cpy(current(), result->name, MAX_LITERAL_NAME);
		next_tok();
	}

	if (strlen(result->name) && current_is(tok_l_paren))
	{
		//function			
		result->kind = decl_func;
		/*(*/
		next_tok();
		parse_function_parameters(&result->data.func);
		/*)*/
		expect_cur(tok_r_paren);
		next_tok();
	}
	else
	{
		// '[...]'
		result->array_sz = opt_parse_array_spec();

		if (type_ref->flags & TF_SC_TYPEDEF)
		{
			//typedef
			parse_register_alias_name(result->name);

			if (current_is(tok_equal))
			{
				parse_err(ERR_SYNTAX, "typedef cannot be initialised");
				ast_destroy_declaration(result);
				return NULL;
			}
		}
		else if (strlen(result->name))
		{
			//variable 
			result->kind = decl_var;

			if (current_is(tok_equal))
			{
				next_tok();
				result->data.var.init_expr = parse_expression();
			}
		}
	}

	if (found_semi)
		*found_semi = current_is(tok_semi_colon);
	if (current_is(tok_semi_colon))
		next_tok();

	result->tokens.end = current();
	return result;
}


ast_declaration_t* try_parse_declaration()
{
	bool found_semi;
	ast_declaration_t* decl = try_parse_declaration_opt_semi(&found_semi);

	if (decl && !found_semi)
	{
		parse_err(ERR_SYNTAX, "expected ';' after declaration of %s", ast_declaration_name(decl));
	}
	return decl;
}
