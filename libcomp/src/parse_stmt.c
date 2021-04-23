#include "parse_internal.h"
#include "diag.h"

#include <stdlib.h>
#include <string.h>

static inline ast_statement_t* _alloc_smnt()
{
	ast_statement_t* result = (ast_statement_t*)malloc(sizeof(ast_statement_t));
	memset(result, 0, sizeof(ast_statement_t));
	result->tokens.start = current();
	return result;
}

static ast_expression_t* _alloc_expr()
{
	ast_expression_t* result = (ast_expression_t*)malloc(sizeof(ast_expression_t));
	memset(result, 0, sizeof(ast_expression_t));
	result->tokens.start = result->tokens.end = current();
	return result;
}


/*
<exp-option> ::= ";" | <expression>

Note: the following is used to parse the 'step' expression in a for loop
<exp-option> ::= ")" | <expression>
*/
ast_expression_t* parse_optional_expression(tok_kind term_tok)
{
	ast_expression_t* expr = NULL;

	if (current_is(term_tok))
	{
		expr = _alloc_expr();
		expr->kind = expr_null;
		return expr;
	}
	expr = parse_expression();
	if (!expr)
		return NULL;
	
	if (!expect_cur(term_tok))
	{
		ast_destroy_expression(expr);
		return NULL;
	}

	return expr;
}

/*
<switch_case> ::= "case" <constant-exp> ":" <statement> | "default" ":" <statement>
*/
ast_switch_case_data_t* parse_switch_case()
{
	bool def = current_is(tok_default);
	next_tok();

	ast_expression_t* expr = NULL;
	if (!def)
	{
		expr = parse_constant_expression();
		if (!expr)
		{
			parse_err(ERR_SYNTAX, "Failed to parse switch case expression");
			return NULL;
		}
	}

	if (!expect_cur(tok_colon))
		return NULL;
	next_tok();

	ast_switch_case_data_t* result = (ast_switch_case_data_t*)malloc(sizeof(ast_switch_case_data_t));
	memset(result, 0, sizeof(ast_switch_case_data_t));
	
	result->const_expr = expr;
	//statement is optional if this is not the default case
	if (!current_is(tok_case) && !current_is(tok_default) && !current_is(tok_r_brace))
		result->statement = parse_statement();
	return result;
}

ast_statement_t* parse_switch_statement()
{
	ast_statement_t* smnt = _alloc_smnt();
	ast_switch_smnt_data_t* data = &smnt->data.switch_smnt;
	smnt->kind = smnt_switch;
	next_tok();

	if (!expect_cur(tok_l_paren))
		goto _parse_switch_err;
	
	next_tok();
	data->expr = parse_expression();
	if(!data->expr)
		goto _parse_switch_err;

	if(!expect_cur(tok_r_paren))
		goto _parse_switch_err;

	next_tok();

	if (current_is(tok_l_brace))
	{
		next_tok();

		ast_switch_case_data_t* last_case = NULL;
		while (current_is(tok_case) || current_is(tok_default))
		{
			bool def = current_is(tok_default);
			ast_switch_case_data_t* case_data = parse_switch_case();
			if (!case_data)
				goto _parse_switch_err;

			if (def)
			{
				if (data->default_case)
				{
					parse_err(ERR_SYNTAX, "Multiple default cases in switch statement");
					goto _parse_switch_err;
				}
				data->default_case = case_data;
			}
			else
			{
				//maintain list order
				if (last_case == NULL)
					data->cases = case_data;
				else
					last_case->next = case_data;
				last_case = case_data;
				data->case_count++;
			}

			if (current_is(tok_r_brace))
			{
				next_tok();
				break;
			}
		}
	}
	//else if (current_is(tok_case) || current_is(tok_default))
	{
	//	parse_switch_case(&result->data.switch_smnt);
	}

	return smnt;

_parse_switch_err:
	ast_destroy_statement(smnt);
	return NULL;
}

//<statement> ::= "return" < exp > ";"
ast_statement_t* parse_return_statement()
{
	token_t* start = current();
	next_tok();
	
	ast_expression_t* expr = parse_optional_expression(tok_semi_colon);
	if (!expr)
	{
		parse_err(ERR_SYNTAX, "Failed to parse expression return statement");
		return NULL;
	}

	if (!expect_cur(tok_semi_colon))
		return NULL;

	next_tok();

	ast_statement_t* smnt = _alloc_smnt();
	smnt->tokens.start = start;
	smnt->tokens.end = current();
	smnt->kind = smnt_return;
	smnt->data.expr = expr;
	return smnt;
}

//<statement> ::= "for" "(" <declaration> <exp-option> ";" <exp-option> ")" <statement>
//<statement> ::= "for" "(" <exp-option> ";" <exp-option> ";" <exp-option> ")" <statement>
ast_statement_t* parse_for_statement()
{
	ast_statement_t* result = _alloc_smnt();
	result->kind = smnt_for_decl;
	ast_for_smnt_data_t* data = &result->data.for_smnt;

	next_tok();
	if (!expect_cur(tok_l_paren))
		goto _parse_for_err;
	next_tok();
	
	parse_on_enter_block();

	//initialisation
	data->decls = try_parse_decl_list(dpc_normal);
	if (data->decls.first)
	{
		expect_cur(tok_semi_colon);
		next_tok();
	}
	else
	{
		result->kind = smnt_for;
		data->init = parse_optional_expression(tok_semi_colon);
		if (!data->init)
		{
			parse_err(ERR_SYNTAX, "Failed to parse init expression in for loop");
			goto _parse_for_err;
		}
		if (!expect_cur(tok_semi_colon))
			goto _parse_for_err;
		next_tok();
	}

	//condition
	data->condition = parse_optional_expression(tok_semi_colon);
	if (!data->condition)
	{
		parse_err(ERR_SYNTAX, "Failed to parse condition expression in for loop");
		goto _parse_for_err;
	}
	if (!expect_cur(tok_semi_colon))
		goto _parse_for_err;
	next_tok();

	if (data->condition->kind == expr_null)
	{
		//if no condition add a constant literal of 1
		data->condition->kind = expr_int_literal;
		data->condition->data.int_literal.val = int_val_one();
	}

	//post
	data->post = parse_optional_expression(tok_r_paren);
	if (!data->post)
	{
		parse_err(ERR_SYNTAX, "Failed to parse post expression in for loop");
		goto _parse_for_err;
	}
	if (!expect_cur(tok_r_paren))
		goto _parse_for_err;
	next_tok();

	data->statement = parse_statement();
	if (!data->statement)
	{
		parse_err(ERR_SYNTAX, "Failed to parse body of for loop");
		goto _parse_for_err;
	}

	parse_on_leave_block();

	return result;
_parse_for_err:
	ast_destroy_statement(result);
	return NULL;
}

//<statement> ::= "if" "(" <exp> ")" <statement> [ "else" <statement> ]
ast_statement_t* parse_if_statement()
{
	token_t* start = current();
	next_tok();
	if (!expect_cur(tok_l_paren))
		return NULL;
	next_tok();

	ast_statement_t* smnt = _alloc_smnt();
	smnt->tokens.start = start;
	smnt->kind = smnt_if;
	smnt->data.if_smnt.condition = parse_expression();
	if (!smnt->data.if_smnt.condition)
		goto _parse_if_err;
	if (!expect_cur(tok_r_paren))
		return NULL;
	next_tok();

	smnt->data.if_smnt.true_branch = parse_statement();
	if (!smnt->data.if_smnt.true_branch)
		goto _parse_if_err;

	if (current_is(tok_else))
	{
		next_tok();
		smnt->data.if_smnt.false_branch = parse_statement();
		if(!smnt->data.if_smnt.false_branch)
			goto _parse_if_err;
	}
	smnt->tokens.end = current();
	return smnt;

_parse_if_err:
	ast_destroy_statement(smnt);
	return NULL;
}

//<statement> :: = "do" <statement> "while" <exp> ";"
ast_statement_t* parse_do_statement()
{
	ast_statement_t* smnt = _alloc_smnt();
	smnt->kind = smnt_do;
	next_tok();
	smnt->data.while_smnt.statement = parse_statement();
	if (!smnt->data.while_smnt.statement)
		goto _parse_do_err;

	if (!expect_cur(tok_while))
		goto _parse_do_err;
	next_tok();
	smnt->data.while_smnt.condition = parse_expression();
	if(!smnt->data.while_smnt.condition)
		goto _parse_do_err;
	if (!expect_cur(tok_semi_colon))
		goto _parse_do_err;
	next_tok();
	return smnt;

_parse_do_err:
	ast_destroy_statement(smnt);
	return NULL;
}

//<statement> :: = "while" "(" <exp> ")" <statement> ";"
ast_statement_t* parse_while_statement()
{
	token_t* start = current();
	next_tok();
	if (!expect_cur(tok_l_paren))
		return NULL;

	next_tok();
	ast_statement_t* smnt = _alloc_smnt();
	smnt->kind = smnt_while;
	smnt->tokens.start = start;
	smnt->data.while_smnt.condition = parse_expression();
	if (!smnt->data.while_smnt.condition)
		goto _parse_while_err;
	if (!expect_cur(tok_r_paren))
		goto _parse_while_err;
	next_tok();
	smnt->data.while_smnt.statement = parse_statement();
	if (!smnt->data.while_smnt.statement)
		goto _parse_while_err;
	
	return smnt;

_parse_while_err:
	ast_destroy_statement(smnt);
	return NULL;
}

/*
<statement> ::= "return" <exp> ";"
			  | <exp-option> ";"
			  | "if" "(" <exp> ")" <statement> [ "else" <statement> ]
			  | "{" { <block-item> } "}
			  | "for" "(" <exp-option> ";" <exp-option> ";" <exp-option> ")" <statement>
			  | "for" "(" <declaration> <exp-option> ";" <exp-option> ")" <statement>
			  | "while" "(" <exp> ")" <statement>
			  | "do" <statement> "while" <exp> ";"
			  | "break" ";"
			  | "continue" ";"
			  | <switch_smnt>
			  | <label_smnt>
*/
ast_statement_t* parse_statement()
{
	token_t* start = current();

	if (current_is(tok_return))
	{
		return parse_return_statement();
	}
	else if (current_is(tok_continue))
	{
		//<statement> ::= "continue" ";"
		next_tok();

		if (!expect_cur(tok_semi_colon))
			return NULL;
		next_tok();

		ast_statement_t* smnt = _alloc_smnt();
		smnt->tokens.start = start;
		smnt->tokens.end = current();
		smnt->kind = smnt_continue;
		return smnt;
	}
	else if (current_is(tok_break))
	{
		//<statement> ::= "break" ";"
		next_tok();

		if (!expect_cur(tok_semi_colon))
			return NULL;
		next_tok();

		ast_statement_t* smnt = _alloc_smnt();
		smnt->tokens.start = start;
		smnt->tokens.end = current();
		smnt->kind = smnt_break;
		return smnt;
	}
	else if (current_is(tok_for))
	{
		return parse_for_statement();
	}
	else if (current_is(tok_while))
	{
		return parse_while_statement();
	}
	else if (current_is(tok_do))
	{
		return parse_do_statement();
	}
	else if (current_is(tok_if))
	{
		return parse_if_statement();
	}
	else if (current_is(tok_l_brace))
	{
		//<statement> ::= "{" { <block - item> } "}"

		ast_statement_t* smnt = _alloc_smnt();
		smnt->tokens.start = start;
		smnt->kind = smnt_compound;
		smnt->data.compound.blocks = parse_block_list();
		smnt->tokens.end = current();
		return smnt;
	}
	else if (current_is(tok_switch))
	{
		return parse_switch_statement();
	}
	else if (current_is(tok_identifier) && next_is(tok_colon))
	{
		//statement ::= <label_smnt>

		ast_statement_t* smnt = _alloc_smnt();
		smnt->tokens.start = start;
		smnt->kind = smnt_label;
		tok_spelling_cpy(current(), smnt->data.label_smnt.label, MAX_LITERAL_NAME);
		next_tok(); //identifier
		next_tok(); //colon
		smnt->data.label_smnt.smnt = parse_statement();
		smnt->tokens.end = current();
		return smnt;
	}
	else if (current_is(tok_goto))
	{
		next_tok();
		if (!expect_cur(tok_identifier))
			return NULL;
		
		ast_statement_t* smnt = _alloc_smnt();
		smnt->tokens.start = start;
		smnt->kind = smnt_goto;		
		tok_spelling_cpy(current(), smnt->data.goto_smnt.label, MAX_LITERAL_NAME);
		next_tok();
		smnt->tokens.end = current();
		return smnt;
	}

	//expression statement
	//<statement ::= <exp-option> ";"
	ast_expression_t* expr = parse_optional_expression(tok_semi_colon);
	if (!expr)
	{
		parse_err(ERR_SYNTAX, "Failed to parse expression");
		return NULL;
	}

	if (!expect_cur(tok_semi_colon))
		return NULL;
	next_tok();

	ast_statement_t* smnt = _alloc_smnt();
	smnt->tokens.start = start;
	smnt->tokens.end = current();
	smnt->kind = smnt_expr;
	smnt->data.expr = expr;
	return smnt;
}
