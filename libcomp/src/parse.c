#include "token.h"
#include "parse.h"
#include "diag.h"

#include <stdio.h>
#include <stdlib.h>

ast_expression_t* parse_expression();
ast_block_item_t* parse_block_item();

static token_t* _cur_tok = NULL;

static inline token_t* current()
{
	return _cur_tok;
}

static inline token_t* next_tok()
{
	_cur_tok = _cur_tok->next;
	return _cur_tok;
}

static inline bool current_is(tok_kind k)
{
	return _cur_tok->kind == k;
}

static inline bool next_is(tok_kind k)
{
	return _cur_tok->next->kind == k;
}

static void expect_cur(tok_kind k)
{
	if (!current_is(k))
		diag_err(current(), ERR_SYNTAX, "Expected %s", tok_kind_name(k));
}

static void expect_next(tok_kind k)
{
	next_tok();
	expect_cur(k);
}

static bool _is_postfix_op(token_t* tok)
{
	switch (tok->kind)
	{
	case tok_plusplus:
	case tok_minusminus:
		return true;
	}
	return false;
}

static bool _is_unary_op(token_t* tok)
{
	switch (tok->kind)
	{
	case tok_minus:
	case tok_tilda:
	case tok_exclaim:
	case tok_plusplus:
	case tok_minusminus:
		return true;
	}
	return false;
}

static op_kind _get_postfix_operator(token_t* tok)
{
	switch (tok->kind)
	{
	case tok_minusminus:
		return op_postfix_dec;
	case tok_plusplus:
		return op_postfix_inc;
	}

	diag_err(current(), ERR_SYNTAX, "Unknown postfix op %s", tok_kind_name(tok->kind));
	return op_unknown;
}

static op_kind _get_unary_operator(token_t* tok)
{
	switch (tok->kind)
	{
	case tok_minus:
		return op_negate;
	case tok_tilda:
		return op_compliment;
	case tok_exclaim:
		return op_not;
	case tok_minusminus:
		return op_prefix_dec;
	case tok_plusplus:
		return op_prefix_inc;
	}

	diag_err(current(),	ERR_SYNTAX, "Unknown unary op %s", tok_kind_name(tok->kind));
	return op_unknown;
}

static op_kind _get_binary_operator(token_t* tok)
{
	switch (tok->kind)
	{
	case tok_minus:
		return op_sub;
	case tok_plus:
		return op_add;
	case tok_star:
		return op_mul;
	case tok_slash:
		return op_div;
	case tok_ampamp:
		return op_and;
	case tok_pipepipe:
		return op_or;
	case tok_equalequal:
		return op_eq;
	case tok_exclaimequal:
		return op_neq;
	case tok_lesser:
		return op_lessthan;
	case tok_lesserequal:
		return op_lessthanequal;
	case tok_greater:
		return op_greaterthan;
	case tok_greaterequal:
		return op_greaterthanequal;
	case tok_lesserlesser:
		return op_shiftleft;
	case tok_greatergreater:
		return op_shiftright;
	case tok_amp:
		return op_bitwise_and;
	case tok_pipe:
		return op_bitwise_or;
	case tok_caret:
		return op_bitwise_xor;
	case tok_percent:
		return op_mod;
	}
	diag_err(current(), ERR_SYNTAX, "Unknown binary op %s", tok_kind_name(tok->kind));
	return op_unknown;
}

static ast_expression_t* _alloc_expr()
{
	ast_expression_t* result = (ast_expression_t*)malloc(sizeof(ast_expression_t));
	memset(result, 0, sizeof(ast_expression_t));
	result->tokens.start = result->tokens.end = current();
	return result;
}

/*
<translation_unit> ::= { <function> }
<function> ::= "int" <id> "(" [ "int" <id> { "," "int" <id> } ] ")" ( "{" { <block-item> } "}" | ";" )
<statement> ::= "return" <exp> ";"
			  | <exp> ";"
			  | "int" <id> [ = <exp>] ";"
<exp> ::= <assignment_exp>
<assignment_exp> ::= <id> "=" <exp> | <conditional-exp>

<conditional-exp> ::= <logical-or-exp> "?" <exp> ":" <conditional-exp>

<logical-or-exp> ::= <logical-and-exp> { "||" <logical-and-exp> }
<logical-and-exp> ::= <bitwise-or> { "&&" <bitwise-or> }

<bitwise-or> :: = <bitwise-xor> { ("|") <bitwise-xor> }
<bitwise-xor> :: = <bitwise-and> { ("^") <bitwise-and> }
<bitwise-and> :: = <equality-exp> { ("&") <equality-exp> }

<equality-exp> ::= <relational-exp> { ("!=" | "==") <relational-exp> }
<relational-exp> ::= <bitshift-exp> { ("<" | ">" | "<=" | ">=") <bitshift-exp> }
<bitshift-exp> ::= <additive-exp> { ("<<" | ">>") <additive-exp> }
<additive-exp> ::= <term> { ("+" | "-") <term> }
<term> ::= <factor> { ("*" | "/") <factor> }
<factor> ::= <function-call> | "(" <exp> ")" | <unary_op> <factor> | <int> | <id> [ <postfix_op> ]
<function-call> ::= id "(" [ <exp> { "," <exp> } ] ")"
<postfix_op> ::= "++" | "--"
<unary_op> ::= "!" | "~" | "-" | "++" | "--"
*/

tok_kind logical_or_ops[] = { tok_pipepipe, tok_invalid };
tok_kind logical_and_ops[] = { tok_ampamp, tok_invalid };
tok_kind bitwise_or_ops[] = { tok_pipe, tok_invalid };
tok_kind bitwise_xor_ops[] = { tok_caret, tok_invalid };
tok_kind bitwise_and_ops[] = { tok_amp, tok_invalid };
tok_kind equality_ops[] = { tok_equalequal, tok_exclaimequal, tok_invalid };
tok_kind relational_ops[] = { tok_lesser, tok_lesserequal, tok_greater, tok_greaterequal, tok_invalid };
tok_kind bitshift_ops[] = { tok_lesserlesser, tok_greatergreater, tok_invalid };
tok_kind additive_ops[] = { tok_plus, tok_minus, tok_invalid };

static bool tok_in_set(tok_kind kind, tok_kind* set)
{
	while (*set != tok_invalid)
	{
		if (kind == *set)
			return true;
		set++;
	}
	return false;
}

typedef ast_expression_t* (*bin_parse_fn)();

ast_expression_t* parse_binary_expression(tok_kind* op_set, bin_parse_fn sub_parse)
{
	token_t* start = current();
	ast_expression_t* expr = sub_parse();
	while (tok_in_set(current()->kind, op_set))
	{
		
		op_kind op = _get_binary_operator(current());
		next_tok();
		ast_expression_t* rhs_expr = sub_parse();

		//our two factors are now expr and rhs_expr
		//build a new binary op expression for expr
		ast_expression_t* cur_expr = expr;
		expr = _alloc_expr();
		expr->tokens.start = start;
		expr->kind = expr_binary_op;
		expr->data.binary_op.operation = op;
		expr->data.binary_op.lhs = cur_expr;
		expr->data.binary_op.rhs = rhs_expr;
	}
	expr->tokens.end = current();
	return expr;
}

/*
<declaration> :: = "int" < id > [= <exp>] ";"
*/
ast_var_decl_t* parse_declaration()
{
	ast_var_decl_t* result = (ast_var_decl_t*)malloc(sizeof(ast_var_decl_t));
	memset(result, 0, sizeof(ast_var_decl_t));
	result->tokens.start = current();
	expect_cur(tok_int);
	next_tok();
	expect_cur(tok_identifier);
	tok_spelling_cpy(current(), result->decl_name, MAX_LITERAL_NAME);
	next_tok();
	if (current_is(tok_equal))
	{
		next_tok();
		result->expr = parse_expression();
	}
	expect_cur(tok_semi_colon);
	next_tok();
	result->tokens.end = current();
	return result;
}

/*
<function-call> ::= id "(" [ <exp> { "," <exp> } ] ")"
*/
ast_expression_t* parse_function_call_expr()
{
	ast_expression_t* expr = NULL;
	if (current_is(tok_identifier) && next_is(tok_l_paren))
	{
		expr = _alloc_expr();
		expr->kind = expr_func_call;
		tok_spelling_cpy(current(), expr->data.func_call.name, MAX_LITERAL_NAME);

		next_tok(); //skip id
		next_tok(); //skip tok_l_paren

		while (!current_is(tok_r_paren))
		{
			if (expr->data.func_call.params)
			{
				expect_cur(tok_comma);
				next_tok();
			}
			ast_expression_t* param_expr = parse_expression();
			param_expr->next = expr->data.func_call.params;
			expr->data.func_call.params = param_expr;
			expr->data.func_call.param_count++;
		}		
		next_tok();
	}
	if (expr)
		expr->tokens.end = current();
	return expr;
}

ast_expression_t* parse_factor();

/*
<postfix-op> ::= <function-call> | <factor> <postfix_op>
*/
ast_expression_t* parse_postfix_expr()
{
	ast_expression_t* expr = parse_function_call_expr();

	if (!expr)
	{
		ast_expression_t* f = parse_factor();

		if (current_is(tok_plusplus))
		{
			expr = _alloc_expr();
			expr->kind = expr_postfix_op;
			expr->data.unary_op.operation = op_postfix_inc;
			expr->data.unary_op.expression = f;
		}
	}
	return expr;
}

/*
<factor> ::= "(" <exp> ")" | <unary_op> <factor> | <int>
<factor> ::= <postfix-op> | "(" <exp> ")" | <unary_op> <factor> | <int> | <id>
*/
ast_expression_t* parse_factor()
{
	ast_expression_t* expr = parse_function_call_expr();

	if (expr)
	{

	}
	else if (current_is(tok_l_paren))
	{
		//<factor ::= "(" <exp> ")"
		next_tok();
		expr = parse_expression();
		expect_cur(tok_r_paren);
		next_tok();
	}
	else if (_is_unary_op(current()))
	{
		//<factor> ::= <unary_op> <factor>
		expr = _alloc_expr();
		expr->kind = expr_unary_op;
		expr->data.unary_op.operation = _get_unary_operator(current());
		next_tok();
		expr->data.unary_op.expression = parse_factor();
	}
	else if (current_is(tok_num_literal))
	{
		//<factor> ::= <int>
		expr = _alloc_expr();
		expr->kind = expr_int_literal;
		expr->data.const_val = (uint32_t)current()->data;
		next_tok();
	}
	else if (current_is(tok_identifier))
	{
		//<factor> ::= <id>
		expr = _alloc_expr();
		expr->kind = expr_var_ref;
		tok_spelling_cpy(current(), expr->data.var_reference.name, MAX_LITERAL_NAME);
		next_tok();

		if (_is_postfix_op(current()))
		{
			expr->tokens.end = current();
			ast_expression_t* operand = expr;
			expr = _alloc_expr();
			expr->tokens = operand->tokens;
			expr->kind = expr_postfix_op;
			expr->data.unary_op.operation = _get_postfix_operator(current());
			expr->data.unary_op.expression = operand; //must be a var ref
			next_tok();			
		}
	}
	else
	{
		diag_err(current(), ERR_SYNTAX, "parse_factor failed");
	}
	if(expr)
		expr->tokens.end = current();
	return expr;
}

/*
<term> ::= <factor> { ("*" | "/" | "%") <factor> }
*/
ast_expression_t* parse_term()
{
	token_t* start = current();
	ast_expression_t* expr = parse_factor();
	while (current()->kind == tok_star || current()->kind == tok_slash || current()->kind == tok_percent)
	{
		op_kind op = _get_binary_operator(current());
		next_tok();
		ast_expression_t* rhs_expr = parse_factor();

		//our two factors are now expr and rhs_expr
		//build a new binary op expression for expr
		ast_expression_t* cur_expr = expr;
		expr = _alloc_expr();		
		expr->tokens.start = start;
		expr->kind = expr_binary_op;
		expr->data.binary_op.operation = op;
		expr->data.binary_op.lhs = cur_expr;
		expr->data.binary_op.rhs = rhs_expr;
	}
	expr->tokens.end = current();
	return expr;
}

/*
<additive-exp> ::= <term> { ("+" | "-") <term> }
*/
ast_expression_t* parse_additive_expr()
{
	return parse_binary_expression(additive_ops, parse_term);
}

/*
<bitshift - exp> :: = <additive - exp>{ ("<<" | ">>") < additive - exp > }
*/
ast_expression_t* parse_bitshift_expr()
{
	return parse_binary_expression(bitshift_ops, parse_additive_expr);
}

/*
<relational - exp> :: = <additive - exp>{ ("<" | ">" | "<=" | ">=") < additive - exp > }
*/
ast_expression_t* parse_relational_expr()
{
	return parse_binary_expression(relational_ops, parse_bitshift_expr);
}

/*
<equality-exp> :: = <relational - exp>{ ("!=" | "==") < relational - exp > }
*/
ast_expression_t* parse_equality_expr()
{
	return parse_binary_expression(equality_ops, parse_relational_expr);
}

/*
<bitwise-and> :: = <equality-exp>{ ("&") < equality-exp > }
*/
ast_expression_t* parse_bitwise_and_expr()
{
	return parse_binary_expression(bitwise_and_ops, parse_equality_expr);
}

/*
<bitwise-xor> :: = <bitwise-and>{ ("^") < bitwise-and > }
*/
ast_expression_t* parse_bitwise_xor_expr()
{
	return parse_binary_expression(bitwise_xor_ops, parse_bitwise_and_expr);
}

/*
<bitwise-or > :: = <bitwise-xor>{ ("|") < bitwise-xor > }
*/
ast_expression_t* parse_bitwise_or_expr()
{
	return parse_binary_expression(bitwise_or_ops, parse_bitwise_xor_expr);
}

/*
<logical-and-exp> ::= <bitwise-or> { "&&" <bitwise-or> }
*/
ast_expression_t* parse_logical_and_expr()
{
	return parse_binary_expression(logical_and_ops, parse_bitwise_or_expr);
}

/*
<logical-or-exp> :: = <logical-and-exp>{ "||" <logical-and-exp > }
*/
ast_expression_t* parse_logical_or_expr()
{
	return parse_binary_expression(logical_or_ops, parse_logical_and_expr);
}

/*
<conditional-exp> :: = <logical-or-exp> "?" <exp> ":" <conditional-exp>
*/
ast_expression_t* parse_conditional_expression()
{
	token_t* start = current();
	ast_expression_t* expr = parse_logical_or_expr();

	if (current_is(tok_question))
	{
		next_tok();
		ast_expression_t* condition = expr;
		
		expr = _alloc_expr();
		expr->tokens.start = start;
		expr->kind = expr_condition;
		expr->data.condition.cond = condition;
		expr->data.condition.true_branch = parse_expression();
		expect_cur(tok_colon);
		next_tok();
		expr->data.condition.false_branch = parse_conditional_expression();
	}
	expr->tokens.end = current();
	return expr;
}

/*
<assignment-exp> ::= <id> "=" <exp> | <conditional-or-exp>
*/
ast_expression_t* parse_assignment_expression()
{
	ast_expression_t* expr;

	if (current_is(tok_identifier) && _cur_tok->next->kind == tok_equal)
	{
		expr = _alloc_expr();
		expr->kind = expr_assign;
		//parse_var
		tok_spelling_cpy(current(), expr->data.assignment.name, MAX_LITERAL_NAME);
		expect_next(tok_equal);
		next_tok();
		expr->data.assignment.expr = parse_expression();
	}
	else
	{
		expr = parse_conditional_expression();
	}
	expr->tokens.end = current();
	return expr;
}

ast_expression_t* parse_expression()
{
	return parse_assignment_expression();
}

/*
<exp-option>
*/
ast_expression_t* parse_optional_expression(tok_kind term_tok)
{
	ast_expression_t* result = NULL;
	if (current_is(term_tok))
	{
		result = _alloc_expr();
		result->kind = expr_null;
	}
	else
	{
		result = parse_expression();
		if (!current_is(term_tok))
		{
			diag_err(current(), ERR_SYNTAX, "Expected %s", tok_kind_name(term_tok));
		}
	}
	//next_tok();
	return result;
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
*/
ast_statement_t* parse_statement()
{
	ast_statement_t* result = (ast_statement_t*)malloc(sizeof(ast_statement_t));
	memset(result, 0, sizeof(ast_statement_t));
	result->tokens.start = current();
	if (current_is(tok_return))
	{
		//<statement> ::= "return" < exp > ";"
		next_tok();
		result->kind = smnt_return;
		result->data.expr = parse_expression();
		expect_cur(tok_semi_colon);
		next_tok();
	}
	else if (current_is(tok_continue))
	{
		//<statement> ::= "continue" ";"
		next_tok();
		result->kind = smnt_continue;
		expect_cur(tok_semi_colon);
		next_tok();
	}
	else if (current_is(tok_break))
	{
		//<statement> ::= "break" ";"
		next_tok();
		result->kind = smnt_break;
		expect_cur(tok_semi_colon);
		next_tok();
	}
	else if (current_is(tok_for))
	{
		expect_next(tok_l_paren);
		next_tok();
		
		if (current_is(tok_int) && next_is(tok_identifier))
		{
			//<statement> ::= "for" "(" <declaration> <exp-option> ";" <exp-option> ")" <statement>
			result->kind = smnt_for_decl;
			result->data.for_smnt.init_decl = parse_declaration();
			result->data.for_smnt.condition = parse_optional_expression(tok_semi_colon);
			expect_cur(tok_semi_colon);
			next_tok();
			result->data.for_smnt.post = parse_optional_expression(tok_r_paren);
		}
		else
		{
			//<statement> ::= "for" "(" <exp-option> ";" <exp-option> ";" <exp-option> ")" <statement>
			result->kind = smnt_for;
			result->data.for_smnt.init = parse_optional_expression(tok_semi_colon);
			expect_cur(tok_semi_colon);
			next_tok();
			result->data.for_smnt.condition = parse_optional_expression(tok_semi_colon);
			expect_cur(tok_semi_colon);
			next_tok();
			result->data.for_smnt.post = parse_optional_expression(tok_r_paren);
		}
		expect_cur(tok_r_paren);
		next_tok();
		result->data.for_smnt.statement = parse_statement();

		if (result->data.for_smnt.condition->kind == expr_null)
		{
			//if no condition add a constant literal of 1
			result->data.for_smnt.condition->kind = expr_int_literal;
			result->data.for_smnt.condition->data.const_val = 1;
		}
	}
	else if (current_is(tok_while))
	{
		//<statement> :: = "while" "(" <exp> ")" <statement> ";"
		expect_next(tok_l_paren);
		next_tok();
		result->kind = smnt_while;
		result->data.while_smnt.condition = parse_expression();
		expect_cur(tok_r_paren);
		next_tok();
		result->data.while_smnt.statement = parse_statement();
	}
	else if (current_is(tok_do))
	{
		next_tok();
		//<statement> :: = "do" <statement> "while" <exp> ";"
		result->kind = smnt_do;
		result->data.while_smnt.statement = parse_statement();
		expect_cur(tok_while);
		next_tok();
		result->data.while_smnt.condition = parse_expression();
		expect_cur(tok_semi_colon);
		next_tok();
	}
	else if (current_is(tok_if))
	{
		//<statement> ::= "if" "(" <exp> ")" <statement> [ "else" <statement> ]
		expect_next(tok_l_paren);
		next_tok();
		result->kind = smnt_if;
		result->data.if_smnt.condition = parse_expression();
		expect_cur(tok_r_paren);
		next_tok();
		result->data.if_smnt.true_branch = parse_statement();

		if (current_is(tok_else))
		{
			next_tok();
			result->data.if_smnt.false_branch = parse_statement();
		}
	}
	else if (current_is(tok_l_brace))
	{
		//<statement> ::= "{" { <block - item> } "}"
		next_tok();
		result->kind = smnt_compound;
		ast_block_item_t* block;
		ast_block_item_t* last_block = NULL;
		do
		{			
			block = parse_block_item();

			if (last_block)
				last_block->next = block;
			else
				result->data.compound.blocks = block;
			last_block = block;
		
		} while (!current_is(tok_r_brace));
		next_tok();
	}
	else
	{
		//<statement ::= <exp-option> ";"
		result->kind = smnt_expr;
		result->data.expr = parse_optional_expression(tok_semi_colon);
		expect_cur(tok_semi_colon);
		next_tok();
	}	
	result->tokens.end = current();
	return result;
}

ast_block_item_t* parse_block_item()
{
	ast_block_item_t* result = (ast_block_item_t*)malloc(sizeof(ast_block_item_t));
	memset(result, 0, sizeof(ast_block_item_t));
	result->tokens.start = current();
	if (current_is(tok_int) && next_is(tok_identifier))
	{
		result->kind = blk_var_def;
		result->var_decl = parse_declaration();
	}
	else
	{
		result->kind = blk_smnt;
		result->smnt = parse_statement();
	}
	result->tokens.end = current();
	return result;
}

/*
<function_params> ::= [ "int" <id> { "," "int" <id> } ]
*/
void parse_function_parameters(ast_function_decl_t* func)
{
	while (current_is(tok_int))
	{
		expect_next(tok_identifier);

		ast_function_param_t* param = (ast_function_param_t*)malloc(sizeof(ast_function_param_t));
		tok_spelling_cpy(current(), param->name, MAX_LITERAL_NAME);
		param->next = func->params;
		func->params = param;
		func->param_count++;
		next_tok();
		if (!current_is(tok_comma))
			break;
		expect_next(tok_int);
	}
}

/*
<function> ::= "int" <id> "(" [ "int" <id> { "," "int" <id> } ] ")" ( "{" { <block-item> } "}" | ";" )
*/
ast_function_decl_t* parse_function_decl()
{
	ast_function_decl_t* result = (ast_function_decl_t*)malloc(sizeof(ast_function_decl_t));
	memset(result, 0, sizeof(ast_function_decl_t));
	result->tokens.start = current();

	/*return type*/
	expect_cur(tok_int);

	/*function name*/
	expect_next(tok_identifier);
	tok_spelling_cpy(current(), result->name, 32);

	/*(*/
	expect_next(tok_l_paren);
	next_tok();

	parse_function_parameters(result);

	/*)*/
	expect_cur(tok_r_paren);
	next_tok();

	if (current_is(tok_semi_colon))
	{
		next_tok();
		//function decl only
		ast_destroy_function_decl(result);
		return NULL;
	}

	/*{*/
	expect_cur(tok_l_brace);

	next_tok();

	ast_block_item_t* last_blk = NULL;

	while (!current_is(tok_r_brace))
	{
		ast_block_item_t* blk = parse_block_item();
		if (last_blk)
			last_blk->next = blk;
		else
			result->blocks = blk;
		last_blk = blk;
		last_blk->next = NULL;
	}
	next_tok();
	result->tokens.end = current();
	return result;
}

//<translation_unit> :: = { <function> }
ast_trans_unit_t* parse_translation_unit(token_t* tok)
{
	_cur_tok = tok;

	ast_trans_unit_t* result = (ast_trans_unit_t*)malloc(sizeof(ast_trans_unit_t));
	memset(result, 0, sizeof(ast_trans_unit_t));
	result->tokens.start = current();

	while (!current_is(tok_eof))
	{
		ast_function_decl_t* fn = parse_function_decl();

		if (fn)
		{
			fn->next = result->functions;
			result->functions = fn;
		}
	}
	result->tokens.end = current();

	expect_cur(tok_eof);
	return result;
}