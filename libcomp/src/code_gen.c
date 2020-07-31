#include "code_gen.h"
#include "var_set.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

void gen_expression(ast_expression_t* expr);
void gen_block_item(ast_block_item_t* bi);

typedef struct
{
	char name[128]; //set if global var
	ast_type_spec_t* type; //target value type
	int32_t stack_offset;

	bool push;
	bool deref;
}lval_data_t;


static write_asm_cb _asm_cb;

static var_set_t* _var_set;
static bool _returned = false;
static ast_function_decl_t* _cur_fun = NULL;
static valid_trans_unit_t* _cur_tl = NULL;
static lval_data_t* _cur_assign_target = NULL;

static const char* _cur_break_label = NULL;
static const char* _cur_cont_label = NULL;

static void _gen_asm(const char* format, ...)
{
	char buff[256];

	va_list args;
	va_start(args, format);
	vsnprintf(buff, 256, format, args);
	va_end(args);

	_asm_cb(buff);
}

static uint32_t _next_label = 0;

static void _make_label_name(char* name)
{
	sprintf(name, "_label%d", ++_next_label);
}

static lval_data_t _create_lval_data()
{
	lval_data_t val = { "", NULL, 0, false, false };
	return val;
}

void gen_logical_binary_expr(ast_expression_t* expr)
{
	char label1[16];
	char label2[16];
	_make_label_name(label1);
	_make_label_name(label2);

	gen_expression(expr->data.binary_op.lhs);

	if (expr->data.binary_op.operation == op_or)
	{
		_gen_asm("cmpl $0, %%eax"); //compare eax to 0
		_gen_asm("je %s", label1);	//eax is the result of the first expression. If true we exit early
		_gen_asm("movl $1, %%eax"); //set eax to 1 (result)
		_gen_asm("jmp %s", label2);

		_gen_asm("%s:", label1);
		gen_expression(expr->data.binary_op.rhs);
		_gen_asm("cmpl $0, %%eax"); //compare eax to 0
		_gen_asm("movl $0, %%eax"); //set eax to 0
		_gen_asm("setne %%al"); //test flags of comparison
		_gen_asm("%s:", label2);
	}
	else if (expr->data.binary_op.operation == op_and)
	{
		_gen_asm("cmpl $0, %%eax"); //compare eax to 0
		_gen_asm("jne %s", label1);
		//eax already 0 (the result)
		_gen_asm("jmp %s", label2); //exit early
		
		_gen_asm("%s:", label1);
		gen_expression(expr->data.binary_op.rhs);
		_gen_asm("cmpl $0, %%eax"); //compare eax to 0
		_gen_asm("movl $0, %%eax"); //set eax to 0
		_gen_asm("setne %%al"); //test flags of comparison
		_gen_asm("%s:", label2);
	}
}

/*

*/
lval_data_t _get_lvalue_addr(ast_expression_t* target)
{
	lval_data_t result = _create_lval_data();
	if (target->kind == expr_identifier)
	{
		var_data_t* var = var_find(_var_set, target->data.var_reference.name);
		if (!var)
		{
			diag_err(target->tokens.start, ERR_UNKNOWN_VAR,
				"reference to unknown variabe %s",
				target->data.var_reference.name);

			return result;
		}

		result.type = var->data.decl->type;

		if (var->kind == var_global)
		{
			result.stack_offset = 0;
			sprintf(result.name, "_var_%s", var->name);
		}
		else
		{
			result.stack_offset = var->bsp_offset;
		}
		return result;
	}
	else if (target->kind == expr_binary_op &&
		target->data.binary_op.operation == op_member_access)
	{
		// a.b
 		lval_data_t a = _get_lvalue_addr(target->data.binary_op.lhs);
		if (a.type)
		{
			assert(target->data.binary_op.rhs->kind == expr_identifier);
			ast_struct_member_t* member = ast_find_struct_member(a.type->user_type_spec,
																target->data.binary_op.rhs->data.var_reference.name);
			assert(member);
			a.stack_offset += member->offset;
			a.type = member->type;
			return a;
		}
	}
	return result;
}

//move the value in eax to target
void gen_assign_expr(token_t* tok, ast_expression_t* target)
{
	// todo

	lval_data_t lval = _get_lvalue_addr(target);

	if (!lval.type)
		return;

	//todo
	if (strlen(lval.name))
	{
		_gen_asm("movl %%eax, %s", lval.name);
	}
	else
	{
		_gen_asm("movl %%eax, %d(%%ebp)", lval.stack_offset);
	}
}

void gen_expression(ast_expression_t* expr)
{
	if (expr->kind == expr_null)
	{
		//null
	}
	else if (expr->kind == expr_func_call)
	{
		ast_declaration_t* decl = idm_find_decl(_cur_tl->identifiers, expr->data.func_call.name, decl_func);
		assert(decl);
		if (!decl)
			return;

		ast_expression_t* param = expr->data.func_call.params;
		ast_declaration_t* param_decl = decl->data.func.params;
		uint32_t pushed = 0;
		while (param)
		{
			if (param_decl->data.var.type->size <= 4)
			{
				pushed += 4;
				gen_expression(param);
				_gen_asm("pushl %%eax");				
			}
			else if (param_decl->data.var.type->size > 4)
			{
				pushed += param_decl->data.var.type->size;

				lval_data_t target = _create_lval_data();
				target.push = true;
				lval_data_t* prev = _cur_assign_target;
				_cur_assign_target = &target;

				gen_expression(param);

				_cur_assign_target = prev;
			}
			else
			{
				assert(false);
			}
			
			param = param->next;
			param_decl = param_decl->next;
		}

		if (decl->data.func.return_type->size > 4)
		{
			//put the address of the target in eax
			if(_cur_assign_target->deref)
				_gen_asm("movl %d(%%ebp), %%eax", _cur_assign_target->stack_offset);
			else
				_gen_asm("leal %d(%%ebp), %%eax", _cur_assign_target->stack_offset);
			//push eax
			_gen_asm("pushl %%eax");
		}
		_gen_asm("call %s", expr->data.func_call.name);
		if (pushed > 0)
		{
			_gen_asm("addl $%d, %%esp", pushed);
		}
	}
	else if (expr->kind == expr_condition)
	{
		char label_false[16];
		char label_end[16];
		_make_label_name(label_false);
		_make_label_name(label_end);

		gen_expression(expr->data.condition.cond); //condition
		_gen_asm("cmpl $0, %%eax"); //was false?
		_gen_asm("je %s", label_false);
		gen_expression(expr->data.condition.true_branch); //true
		_gen_asm("jmp %s", label_end);
		_gen_asm("%s:", label_false);
		gen_expression(expr->data.condition.false_branch);
		_gen_asm("%s:", label_end);
	}
	else if (expr->kind == expr_assign)
	{
		lval_data_t lval = _get_lvalue_addr(expr->data.assignment.target);
		
		if (!lval.type)
		{
			return;
		}
		lval_data_t* prev_target = _cur_assign_target;
		_cur_assign_target = &lval;

		gen_expression(expr->data.assignment.expr);

		//move the value in eax to target
		if (lval.type->size > 4)
		{
			
		}
		else if (lval.type->size == 4)
		{
			if (strlen(lval.name))
				_gen_asm("movl %%eax, %s", lval.name); //global
			else
				_gen_asm("movl %%eax, %d(%%ebp)", lval.stack_offset);
		}
		else if (lval.type->size == 2)
		{
			if (strlen(lval.name))
				_gen_asm("movw %%ax, %s", lval.name); //global
			else
				_gen_asm("movw %%ax, %d(%%ebp)", lval.stack_offset);
		}
		else if (lval.type->size == 1)
		{
			if (strlen(lval.name))
				_gen_asm("movb %%al, %s", lval.name); //global
			else
				_gen_asm("movb %%al, %d(%%ebp)", lval.stack_offset);
		}
		else
		{
			assert(false);
		}		
		_cur_assign_target = prev_target;

		if(_cur_assign_target)
			gen_expression(expr->data.assignment.target);		
	}
	else if (expr->kind == expr_identifier)
	{
		/*
		Variable reference		
		*/
		lval_data_t lval = _get_lvalue_addr(expr);
		if (lval.type)
		{
			// code: move the value of the(global or stack) variable to eax
			if (lval.type->size == 4)
			{				
				if (strlen(lval.name))
					_gen_asm("movl %s, %%eax", lval.name);
				else
					_gen_asm("movl %d(%%ebp), %%eax", lval.stack_offset);
			}
			else if (lval.type->size == 2)
			{
				if (strlen(lval.name))
					_gen_asm("movzwl %s, %%eax", lval.name);
				else
					_gen_asm("movzwl %d(%%ebp), %%eax", lval.stack_offset);
			}
			else if (lval.type->size == 1)
			{
				if (strlen(lval.name))
					_gen_asm("movzbl %s, %%eax", lval.name);
				else
					_gen_asm("movzbl %d(%%ebp), %%eax", lval.stack_offset);
			}
			else if (lval.type->size > 4)
			{
				if (_cur_assign_target->deref)
				{
					//the destination address is in the stack at stack_offset
					//load the address into eax
					_gen_asm("movl %d(%%ebp), %%eax", _cur_assign_target->stack_offset);

					// code: copy the bytes from source to the address in eax
					size_t sz = lval.type->size;
					int source_stack = lval.stack_offset;
					uint32_t dest_off = 0;
					while (sz > 0)
					{
						_gen_asm("movl %d(%%ebp), %%edx", source_stack);
						_gen_asm("movl %%edx, %d(%%eax)", dest_off);
						source_stack += 4;
						dest_off += 4;
						sz -= 4;
					}
				}
				else if (_cur_assign_target->push)
				{
					int source_stack = lval.stack_offset + lval.type->size -4;
					size_t sz = lval.type->size;
					while (sz > 0)
					{
						_gen_asm("pushl %d(%%ebp)", source_stack);
						source_stack -= 4;
						sz -= 4;
					}
				}
				else
				{
					//the destination is a stack offset
					size_t sz = lval.type->size;
					int source_stack = lval.stack_offset;
					int32_t dest_off = _cur_assign_target->stack_offset;
					while (sz > 0)
					{
						_gen_asm("movl %d(%%ebp), %%edx", source_stack);
						_gen_asm("movl %%edx, %d(%%ebp)", dest_off);
						source_stack += 4;
						dest_off += 4;
						sz -= 4;
					}
				}
			}
			else
			{
				assert(false);
			}
		}
	}
	else if (expr->kind == expr_int_literal)
	{
		_gen_asm("movl $%d, %%eax", expr->data.int_literal.value);
	}
	else if (expr->kind == expr_unary_op)
	{
		ast_expression_t* param = expr->data.unary_op.expression;
		gen_expression(param);
		switch (expr->data.unary_op.operation)
		{
		case op_negate:
			_gen_asm("neg %%eax");
			break;
		case op_compliment:
			_gen_asm("not %%eax");
			break;
		case op_not:
			_gen_asm("cmpl $0, %%eax"); //compare eax to 0
			_gen_asm("movl $0, %%eax"); //set eax to 0
			_gen_asm("sete %%al"); //if eax was 0 in cmpl, set al to 1
			break;
		case op_prefix_inc:
			assert(param->kind == expr_identifier);
			_gen_asm("incl %%eax");
			gen_assign_expr(expr->tokens.start, param);
			break;
		case op_prefix_dec:
			assert(expr->data.unary_op.expression->kind == expr_identifier);
			_gen_asm("decl %%eax");
			gen_assign_expr(expr->tokens.start, param);
			break;
		}
	}
	else if (expr->kind == expr_postfix_op)
	{
		assert(expr->data.unary_op.expression->kind == expr_identifier);
		ast_expression_t* param = expr->data.unary_op.expression;
		switch (expr->data.unary_op.operation)
		{
		case op_postfix_inc:
			//get the value of the parameter
			gen_expression(param);
			//save the current value
			_gen_asm("pushl %%eax");
			//increment
			_gen_asm("incl %%eax");
			//assign incremented value back to variable
			gen_assign_expr(expr->tokens.start, param);
			//pop the un-incremented value back into eax
			_gen_asm("popl %%eax");
			break;
		case op_postfix_dec:
			gen_expression(param);
			_gen_asm("pushl %%eax");
			_gen_asm("decl %%eax");
			gen_assign_expr(expr->tokens.start, param);
			_gen_asm("popl %%eax");
			break;
		}
	}
	else if (expr->kind == expr_binary_op)
	{
		if (expr->data.binary_op.operation == op_and ||
			expr->data.binary_op.operation == op_or)
		{
			gen_logical_binary_expr(expr);
			return;
		}
		else if (expr->data.binary_op.operation == op_member_access)
		{
			lval_data_t lval = _get_lvalue_addr(expr);
			_gen_asm("movl %d(%%ebp), %%eax", lval.stack_offset);
			return;
		}

		gen_expression(expr->data.binary_op.rhs);
		_gen_asm("push %%eax");
		gen_expression(expr->data.binary_op.lhs);
		_gen_asm("pop %%ecx");

		switch (expr->data.binary_op.operation)
		{
		case op_add:
			_gen_asm("addl %%ecx, %%eax");
			break;
		case op_sub:
			_gen_asm("subl %%ecx, %%eax");
			break;
		case op_mul:
			_gen_asm("imul %%ecx, %%eax");
			break;
		case op_div:
			_gen_asm("xor %%edx, %%edx");
			_gen_asm("cdq");
			_gen_asm("idivl %%ecx");
			break;
		case op_shiftleft:
			_gen_asm("sall %%cl, %%eax");
			break;
		case op_shiftright:
			_gen_asm("sarl %%cl, %%eax");
			break;
		case op_eq:
			_gen_asm("cmpl %%ecx, %%eax"); //compare eax to ecx
			_gen_asm("movl $0, %%eax"); //set eax to 0
			_gen_asm("sete %%al"); //test flags of comparison
			break;
		case op_neq:
			_gen_asm("cmpl %%ecx, %%eax"); //compare eax to ecx
			_gen_asm("movl $0, %%eax"); //set eax to 0
			_gen_asm("setne %%al"); //test flags of comparison
			break;
		case op_greaterthanequal:
			_gen_asm("cmpl %%ecx, %%eax"); //compare eax to ecx
			_gen_asm("movl $0, %%eax"); //set eax to 0
			_gen_asm("setge %%al"); //test flags of comparison
			break;
		case op_greaterthan:
			_gen_asm("cmpl %%ecx, %%eax"); //compare eax to ecx
			_gen_asm("movl $0, %%eax"); //set eax to 0
			_gen_asm("setg %%al"); //test flags of comparison
			break;
		case op_lessthanequal:
			_gen_asm("cmpl %%ecx, %%eax"); //compare eax to ecx
			_gen_asm("movl $0, %%eax"); //set eax to 0
			_gen_asm("setle %%al"); //test flags of comparison
			break;
		case op_lessthan:
			_gen_asm("cmpl %%ecx, %%eax"); //compare eax to ecx
			_gen_asm("movl $0, %%eax"); //set eax to 0
			_gen_asm("setl %%al"); //test flags of comparison
			break;
		case op_bitwise_and:
			_gen_asm("andl %%ecx, %%eax"); //and eax to ecx, store in eax
			break;
		case op_bitwise_or:
			_gen_asm("orl %%ecx, %%eax"); //and eax to ecx, store in eax
			break;
		case op_bitwise_xor:
			_gen_asm("xorl %%ecx, %%eax"); //and eax to ecx, store in eax
			break;
		case op_mod:
			_gen_asm("xor %%edx, %%edx");
			_gen_asm("cdq");
			_gen_asm("idivl %%ecx");
			_gen_asm("movl %%edx, %%eax");
			break;
		}
	}
}

void gen_var_decl(ast_var_decl_t* var_decl)
{
	var_data_t* var = var_decl_stack_var(_var_set, var_decl);
	assert(var);
	if (var_decl->expr)
	{
		lval_data_t lval = _create_lval_data();
		lval.type = var_decl->type;
		lval.stack_offset = var->bsp_offset;

		lval_data_t* prev_target = _cur_assign_target;
		_cur_assign_target = &lval;

		gen_expression(var_decl->expr);

		if (lval.type->size == 4)
		{		
			_gen_asm("movl %%eax, %d(%%ebp)", var->bsp_offset);
		}
		_cur_assign_target = prev_target;
	}
	else
	{
		for (uint32_t i = 0; i < var_decl->type->size / 4; i++)
		{
			_gen_asm("movl $0, %d(%%ebp)", var->bsp_offset + i * 4);
		}
	}
}

void gen_scope_block_enter()
{
	var_enter_block(_var_set);	
}

void gen_scope_block_leave()
{
	var_leave_block(_var_set);
}

void gen_return_statement(ast_statement_t* smnt)
{
	if (_cur_fun->return_type->size > 4)
	{
		lval_data_t* prev = _cur_assign_target;
		lval_data_t ret_val = _create_lval_data();
		ret_val.stack_offset = 8;
		ret_val.deref = true;
		ret_val.type = _cur_fun->return_type;
		
		_cur_assign_target = &ret_val;
		
		gen_expression(smnt->data.expr);
		_cur_assign_target = prev;		
	}
	else
	{
		gen_expression(smnt->data.expr);
	}

	//function epilogue
	if (_cur_fun->return_type->size > 4)
	{
		_gen_asm("movl 8(%%ebp), %%eax"); //restore return value address in eax
		_gen_asm("leave");
		_gen_asm("ret $4");
	}
	else
	{
		_gen_asm("leave");
		_gen_asm("ret");
	}
	_returned = true;
}

void gen_statement(ast_statement_t* smnt)
{
	const char* cur_break = _cur_break_label;
	const char* cur_cont = _cur_cont_label;

	if (smnt->kind == smnt_return)
	{
		gen_return_statement(smnt);
	}
	else if (smnt->kind == smnt_expr)
	{
		gen_expression(smnt->data.expr);
	}
	else if (smnt->kind == smnt_if)
	{
		char label_false[16];
		char label_end[16];
		_make_label_name(label_false);
		_make_label_name(label_end);

		gen_expression(smnt->data.if_smnt.condition); //condition
		_gen_asm("cmpl $0, %%eax"); //was false?
		_gen_asm("je %s", label_false);
		gen_statement(smnt->data.if_smnt.true_branch); //true
		_gen_asm("jmp %s", label_end);
		_gen_asm("%s:", label_false);
		if(smnt->data.if_smnt.false_branch)
			gen_statement(smnt->data.if_smnt.false_branch);
		_gen_asm("%s:", label_end);
	}
	else if (smnt->kind == smnt_compound)
	{
		gen_scope_block_enter();

		ast_block_item_t* blk = smnt->data.compound.blocks;
		while (blk)
		{
			gen_block_item(blk);
			blk = blk->next;
		}

		gen_scope_block_leave();
	}
	else if (smnt->kind == smnt_while)
	{
		char label_start[16];
		char label_end[16];
		_make_label_name(label_start);
		_make_label_name(label_end);

		_cur_break_label = label_end;
		_cur_cont_label = label_start;

		_gen_asm("%s:", label_start);
		gen_expression(smnt->data.while_smnt.condition);
		_gen_asm("cmpl $0, %%eax"); //was false?
		_gen_asm("je %s", label_end);
		gen_statement(smnt->data.while_smnt.statement);
		_gen_asm("jmp %s", label_start);
		_gen_asm("%s:", label_end);
	}
	else if (smnt->kind == smnt_do)
	{
		char label_start[16];
		char label_end[16];
		char label_cont[16];
		_make_label_name(label_start);
		_make_label_name(label_end);
		_make_label_name(label_cont);

		_cur_break_label = label_end;
		_cur_cont_label = label_cont;

		_gen_asm("%s:", label_start);
		gen_statement(smnt->data.while_smnt.statement);
		_gen_asm("%s:", label_cont);
		gen_expression(smnt->data.while_smnt.condition);
		_gen_asm("cmpl $0, %%eax"); //was false?
		_gen_asm("je %s", label_end);
		_gen_asm("jmp %s", label_start);
		_gen_asm("%s:", label_end);
	}
	else if (smnt->kind == smnt_for)
	{
		ast_for_smnt_data_t* f_data = &smnt->data.for_smnt;

		char label_start[16];
		char label_end[16];
		char label_cont[16];
		_make_label_name(label_start);
		_make_label_name(label_end);
		_make_label_name(label_cont);

		gen_scope_block_enter();

		if (f_data->init)
			gen_expression(f_data->init);

		_cur_break_label = label_end;
		_cur_cont_label = label_cont;

		_gen_asm("%s:", label_start);
		gen_expression(f_data->condition);
		_gen_asm("cmpl $0, %%eax"); //was false?
		_gen_asm("je %s", label_end);
		gen_statement(f_data->statement);
		_gen_asm("%s:", label_cont);
		if (f_data->post)
			gen_expression(f_data->post);
		_gen_asm("jmp %s", label_start);
		_gen_asm("%s:", label_end);

		gen_scope_block_leave();
	}
	else if (smnt->kind == smnt_for_decl)
	{
		ast_for_smnt_data_t* f_data = &smnt->data.for_smnt;

		char label_start[16];
		char label_end[16];
		char label_cont[16];
		_make_label_name(label_start);
		_make_label_name(label_end);
		_make_label_name(label_cont);

		gen_scope_block_enter();

		if (f_data->init_decl)
			gen_var_decl(&f_data->init_decl->data.var);

		_cur_break_label = label_end;
		_cur_cont_label = label_cont;

		_gen_asm("%s:", label_start);
		gen_expression(f_data->condition);
		_gen_asm("cmpl $0, %%eax"); //was false?
		_gen_asm("je %s", label_end);
		gen_statement(f_data->statement);
		_gen_asm("%s:", label_cont);
		if (f_data->post)
			gen_expression(f_data->post);
		_gen_asm("jmp %s", label_start);
		_gen_asm("%s:", label_end);
		gen_scope_block_leave();
	}
	else if (smnt->kind == smnt_break)
	{
		if (!_cur_break_label)
			diag_err(smnt->tokens.start, ERR_SYNTAX, "Invalid break");
		_gen_asm("jmp %s", _cur_break_label);
	}
	else if (smnt->kind == smnt_continue)
	{
	if (!_cur_cont_label)
		diag_err(smnt->tokens.start, ERR_SYNTAX, "Invalid continue");
	_gen_asm("jmp %s", _cur_cont_label);
	}
	//if, break, continue,  etc

	_cur_break_label = cur_break;
	_cur_cont_label = cur_cont;
}

void gen_block_item(ast_block_item_t* bi)
{
	if (bi->kind == blk_smnt)
	{
		gen_statement(bi->smnt);
	}
	else if (bi->kind == blk_decl)
	{
		if(bi->decl->kind == decl_var)
			gen_var_decl(&bi->decl->data.var);
	}
}

void gen_function(ast_function_decl_t* fn)
{	
	_returned = false;
	_cur_fun = fn;

	_gen_asm(".globl %s", fn->name);
	_gen_asm("%s:", fn->name);

	//function prologue
	_gen_asm("push %%ebp");
	_gen_asm("movl %%esp, %%ebp");

	if (fn->required_stack_size > 0)
	{
		//align stack on 4 byte boundry
		uint32_t sz = (fn->required_stack_size + 0x03) & ~0x03;
		_gen_asm("subl $%d, %%esp", sz);
	}

	ast_block_item_t* blk = fn->blocks;

	while (blk)
	{
		gen_block_item(blk);
		blk = blk->next;
	}

	if (!_returned)
	{
		//function epilogue
		_gen_asm("movl $0, %%eax");
		_gen_asm("leave");

		_gen_asm("ret");
	}
	_gen_asm("\n");
	_gen_asm("\n");

	_cur_fun = NULL;
}

void gen_global_var(ast_var_decl_t* var)
{	
	_gen_asm(".globl _var_%s", var->name); //export symbol
	if (var->expr && var->expr->data.int_literal.value != 0)
	{
		//initialised var goes in .data section
		_gen_asm(".data"); //data section
		_gen_asm(".align 4");
		_gen_asm("_var_%s:", var->name); //label
		_gen_asm(".long %d", var->expr->data.int_literal.value); //data and init value
	}
	else
	{
		//0 or uninitialised var goes in .BSS section
		_gen_asm(".bss"); //bss section
		_gen_asm(".align 4");
		_gen_asm("_var_%s:", var->name); //label
		_gen_asm(".zero 4"); //data length		
	}
	_gen_asm(".text"); //back to text section
	_gen_asm("\n");
}

void code_gen(valid_trans_unit_t* tl, write_asm_cb cb)
{
	_asm_cb = cb;
	_cur_tl = tl;
	_var_set = var_init_set();

	ast_declaration_t* var = tl->variables;
	while (var)
	{
		var_decl_global_var(_var_set, &var->data.var);
		gen_global_var(&var->data.var);
		var = var->next;
	}

	ast_declaration_t* fn = tl->functions;
	while(fn)
	{ 
		if (fn->data.func.blocks)
		{
			var_enter_function(_var_set, &fn->data.func);
			gen_function(&fn->data.func);
			var_leave_function(_var_set);
		}

		fn = fn->next;
	}
	var_destory_set(_var_set);
	_cur_tl = NULL;
	_var_set = NULL;
}