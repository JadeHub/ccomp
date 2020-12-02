#include "pp.h"
#include "pp_internal.h"
#include "diag.h"
#include "ast.h"
#include "int_val.h"

#include <string.h>

typedef struct
{
	int_val_t val;
	//uint32_t value;
	token_range_t range;
}const_expr_result_t;

static token_t* _eval_value(token_t* tok, const_expr_result_t* result, pp_context_t* pp);

static uint32_t _op_precedence(tok_kind kind)
{
	switch (kind)
	{
	case tok_percent:
	case tok_slash:
	case tok_star:                 return 14;
	case tok_plus:
	case tok_minus:                return 13;
	case tok_lesserlesser:
	case tok_greatergreater:       return 12;
	case tok_lesserequal:
	case tok_lesser:	
	case tok_greaterequal:
	case tok_greater:              return 11;
	case tok_exclaimequal:
	case tok_equalequal:           return 10;
	case tok_amp:                  return 9;
	case tok_caret:                return 8;
	case tok_pipe:                 return 7;
	case tok_ampamp:               return 6;
	case tok_pipepipe:             return 5;
	case tok_question:             return 4;
	case tok_comma:                return 3;
	case tok_colon:                return 2;
	case tok_r_paren:              return 0;// Lowest priority, end of expr.
	case tok_eof:                  return 0;// Lowest priority, end of directive.
	}
	return ~0;
}

static inline bool _expect_kind(token_t* tok, tok_kind kind)
{
	if (tok->kind != kind)
	{
		diag_err(tok, ERR_SYNTAX, "syntax error: expected '%s' before '%s'",
			tok_kind_spelling(kind), diag_tok_desc(tok));
		return false;
	}
	return true;
}

/*
Evaluate the '+ B' in 'A + B' where result contains the evaluation of A
*/
static token_t* _eval_sub_expr(token_t* tok, 
		const_expr_result_t* result, 
		uint32_t min_prec,
		pp_context_t* pp)
{
	uint32_t peek_prec = _op_precedence(tok->kind);

	while (true)
	{
		if (peek_prec < min_prec)
		{
			//return to higher levels of recursion
			return tok;
		}

		//get the op
		tok_kind op = tok->kind;
		tok = tok->next;

		//eval the RHS
		const_expr_result_t rhs_result;
		rhs_result.val = int_val_zero();
		tok = _eval_value(tok, &rhs_result, pp);
		if (!tok) return NULL;

		//get the precedence of the op after RHS
		uint32_t this_prec = peek_prec;
		peek_prec = _op_precedence(tok->kind);
		if (tok == result->range.end)
			peek_prec = 0;

		if (peek_prec == ~0)
		{
			//diag
			return NULL;
		}
		
		uint32_t rhs_prec;
		if (op == tok_question)
			rhs_prec = _op_precedence(tok_comma);
		else
			rhs_prec = this_prec + 1;

		if (peek_prec >= rhs_prec)
		{
			tok = _eval_sub_expr(tok, &rhs_result, rhs_prec, pp);
			if (!tok) return NULL;
			peek_prec = _op_precedence(tok->kind);
		}

		switch (op)
		{
		//arithmetic
		case tok_plus:
			result->val = int_val_binary_op(&result->val, &rhs_result.val, op_add);
			break;
		case tok_minus:
			result->val = int_val_binary_op(&result->val, &rhs_result.val, op_sub);
			break;
		case tok_star:
			result->val = int_val_binary_op(&result->val, &rhs_result.val, op_mul);
			break;
		case tok_slash:
			result->val = int_val_binary_op(&result->val, &rhs_result.val, op_div);
			break;
		case tok_percent:
			result->val = int_val_binary_op(&result->val, &rhs_result.val, op_mod);
			break;
		//shift
		case tok_lesserlesser:
			result->val = int_val_binary_op(&result->val, &rhs_result.val, op_shiftleft);
			break;
		case tok_greatergreater:
			result->val = int_val_binary_op(&result->val, &rhs_result.val, op_shiftright);
			break;
		//logical
		case tok_equalequal:
			result->val = int_val_binary_op(&result->val, &rhs_result.val, op_eq);
			break;
		case tok_exclaimequal:
			result->val = int_val_binary_op(&result->val, &rhs_result.val, op_neq);
			break;
		case tok_lesserequal:
			result->val = int_val_binary_op(&result->val, &rhs_result.val, op_lessthanequal);
			break;
		case tok_lesser:
			result->val = int_val_binary_op(&result->val, &rhs_result.val, op_lessthan);
			break;
		case tok_greaterequal:
			result->val = int_val_binary_op(&result->val, &rhs_result.val, op_greaterthanequal);
			break;
		case tok_greater:
			result->val = int_val_binary_op(&result->val, &rhs_result.val, op_greaterthan);
			break;
		case tok_ampamp:
			result->val = int_val_binary_op(&result->val, &rhs_result.val, op_and);
			break;
		case tok_pipepipe:
			result->val = int_val_binary_op(&result->val, &rhs_result.val, op_or);
			break;
		//bitwise
		case tok_amp:
			result->val = int_val_binary_op(&result->val, &rhs_result.val, op_bitwise_and);
			break;
		case tok_caret:
			result->val = int_val_binary_op(&result->val, &rhs_result.val, op_bitwise_xor);
			break;
		case tok_pipe:
			result->val = int_val_binary_op(&result->val, &rhs_result.val, op_bitwise_or);
			break;
		}

		if (int_val_required_width(&result->val) > 32)
		{
			//overflow
			diag_err(tok, ERR_VALUE_OVERFLOW, "expression overflowed");
			return NULL;
		}
	}

	return tok;
}

static token_t* _eval_defined(token_t* tok, const_expr_result_t* result, pp_context_t* pp)
{
	bool paren = tok->kind == tok_l_paren;
	if (paren)
		tok = tok->next;

	if (!_expect_kind(tok, tok_identifier)) return NULL;

	result->val = sht_lookup(pp->defs, tok->data.str) ? int_val_one() : int_val_zero();
	tok = tok->next;
	if (paren)
	{
		if (!_expect_kind(tok, tok_r_paren)) return NULL;
		tok = tok->next;
	}
	return tok;
}

static token_t* _eval_value(token_t* tok, const_expr_result_t* result, pp_context_t* pp)
{
	switch (tok->kind)
	{
	case tok_identifier:
	{
		//handle defined X, defined (X)
		char* name = tok->data.str;
		
		if (strcmp(name, "defined") == 0)
			return _eval_defined(tok->next, result, pp);

		diag_err(tok, ERR_SYNTAX, "unexpected identifier: '%s' in constant expression", name);
		return NULL;
	}
	case tok_num_literal:
		//result->value = int_val_as_uint32(&tok->data.int_val);
		result->val = tok->data.int_val;
		return tok->next;
		break;
	case tok_exclaim:
		tok = tok->next;
		tok = _eval_value(tok, result, pp);
		result->val = int_val_unary_op(&result->val, op_not);
//		result->value = !result->value;
		return tok;
	case tok_minus:
		tok = tok->next;
		tok = _eval_value(tok, result, pp);
		//result->value = -result->value;
		result->val = int_val_unary_op(&result->val, op_negate);
		return tok;
	case tok_plus:
		tok = tok->next;
		tok = _eval_value(tok, result, pp);
		//result->value = result->value;
		return tok;
	case tok_tilda:
		tok = tok->next;
		tok = _eval_value(tok, result, pp);
		//result->value = ~result->value;
		result->val = int_val_unary_op(&result->val, op_compliment);
		return tok;
		break;
	case tok_l_paren:
		tok = tok->next;		
		tok = _eval_value(tok, result, pp);
		if (!tok) return NULL;
		//if something like (X), just discard the parens
		if (tok->kind == tok_r_paren)
			return tok->next;

		//otherwise we have something like (X+Y) and result contains the eval of X
		if (!(tok = _eval_sub_expr(tok, result, 1, pp))) return NULL;

		if (!_expect_kind(tok, tok_r_paren)) return NULL;

		return tok->next;
	}
	return NULL;
}

bool pre_proc_eval_expr(pp_context_t* pp, token_range_t range, uint32_t* val)
{
	const_expr_result_t result;
	result.val = int_val_zero();
	result.range = range;

	token_t* tok = _eval_value(range.start, &result, pp);
	if (!tok)
		return false;

	if (tok != range.end)
	{
		tok = _eval_sub_expr(tok, &result, _op_precedence(tok_question), pp);
	}

	if (tok == range.end)
	{
		*val = int_val_as_uint32(&result.val);
		return true;
	}
	
	return false;
}