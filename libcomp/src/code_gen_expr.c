#include "code_gen.h"

#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>

bool lval = false;


static bool _is_unsigned_int_type(ast_type_spec_t* type)
{
	return type->kind == type_uint8 ||
		type->kind == type_uint16 ||
		type->kind == type_uint32;
}

static const char* _promoting_mov_instr(ast_type_spec_t* type)
{
	if (type->size == 4)
		return "movl";

	if (type->size == 2)
		return _is_unsigned_int_type(type) ? "movzwl" : "movswl";

	if (type->size == 1)
		return _is_unsigned_int_type(type) ? "movzbl" : "movsbl";

	assert(false);
	return "";
}

static inline bool is_unary_op(ast_expression_t* expr, op_kind op)
{
	return expr->kind == expr_unary_op && expr->data.unary_op.operation == op;
}

static inline bool is_binary_op(ast_expression_t* expr, op_kind op)
{
	return expr->kind == expr_binary_op && expr->data.binary_op.operation == op;
}

void gen_assignment(ast_expression_t* expr)
{
	gen_annotate_start("assignment expression");

	gen_annotate("lval");
	bool lval_prev = lval;
	lval = true;
	gen_expression(expr->data.binary_op.lhs);
	gen_asm("push %%eax");
	
	gen_annotate("rval");
	lval = false;
	gen_expression(expr->data.binary_op.rhs);
	lval = lval_prev;
	gen_asm("pop %%edx");

	//if the target is a user defined type we assume eax and edx are pointers
	if (expr->sema.result_type->kind == type_user)
	{
		gen_annotate("copy user defined type by value");
		uint32_t offset = 0;
		while (offset < expr->sema.result_type->size)
		{
			gen_asm("movl %d(%%eax), %%ecx", offset);
			gen_asm("movl %%ecx, %d(%%edx)", offset);
			offset += 4;
		}
	}
	else
	{
		gen_annotate("store result");
		gen_asm("movl %%eax, (%%edx)");
	}

	if (lval_prev)
	{
		//we need to leave the address in eax
		gen_asm("movl %%edx, %%eax");
	}

	gen_annotate_end();
}

void gen_op_assign(ast_expression_t* expr)
{
	gen_annotate("op assign operation %s", ast_op_name(expr->data.binary_op.operation));
	// x+= 5;

	gen_annotate("lval");
	bool lval_prev = lval;
	lval = true;
	gen_expression(expr->data.binary_op.lhs);
	gen_asm("push %%eax");	//eax contains the address of lhs

	gen_annotate("rval");
	lval = false;
	gen_expression(expr->data.binary_op.rhs);
	lval = lval_prev;
	gen_annotate("move rhs to ecx");
	gen_asm("movl %%eax, %%ecx");

	gen_annotate("pop lhs address into ebx");
	gen_asm("pop %%ebx");

	gen_annotate("move lhs value into eax");
	gen_asm("movl (%%ebx), %%eax");

	gen_annotate("operation");
	//eax contains rhs, edx contains the address of lhs
	switch (expr->data.binary_op.operation)
	{
	case op_add_assign:
		gen_asm("addl %%ecx, %%eax");
		break;
	case op_sub_assign:
		gen_asm("subl %%ecx, %%eax");
		break;
	case op_mul_assign:
		gen_asm("imul %%ecx, %%eax");
		break;
	case op_div_assign:
		//clear edx
		gen_asm("xor %%edx, %%edx");
		//sign extend eax into edx:eax
		gen_asm("cdq");
		gen_asm("idivl %%ecx");
		break;
	case op_left_shift_assign:
		gen_asm("sall %%cl, %%eax");
		break;
	case op_right_shift_assign:
		gen_asm("sarl %%cl, %%eax");
		break;
	case op_eq:
		gen_asm("cmpl %%ecx, %%eax"); //compare eax to ecx
		gen_asm("movl $0, %%eax"); //set eax to 0
		gen_asm("sete %%al"); //test flags of comparison
		break;
	case op_neq:
		gen_asm("cmpl %%ecx, %%eax"); //compare eax to ecx
		gen_asm("movl $0, %%eax"); //set eax to 0
		gen_asm("setne %%al"); //test flags of comparison
		break;
	case op_greaterthanequal:
		gen_asm("cmpl %%ecx, %%eax"); //compare eax to ecx
		gen_asm("movl $0, %%eax"); //set eax to 0
		gen_asm("setge %%al"); //test flags of comparison
		break;
	case op_greaterthan:
		gen_asm("cmpl %%ecx, %%eax"); //compare eax to ecx
		gen_asm("movl $0, %%eax"); //set eax to 0
		gen_asm("setg %%al"); //test flags of comparison
		break;
	case op_lessthanequal:
		gen_asm("cmpl %%ecx, %%eax"); //compare eax to ecx
		gen_asm("movl $0, %%eax"); //set eax to 0
		gen_asm("setle %%al"); //test flags of comparison
		break;
	case op_lessthan:
		gen_asm("cmpl %%ecx, %%eax"); //compare eax to ecx
		gen_asm("movl $0, %%eax"); //set eax to 0
		gen_asm("setl %%al"); //test flags of comparison
		break;
	case op_and_assign:
		gen_asm("andl %%ecx, %%eax"); //and eax to ecx, store in eax
		break;
	case op_or_assign:
		gen_asm("orl %%ecx, %%eax"); //and eax to ecx, store in eax
		break;
	case op_xor_assign:
		gen_asm("xorl %%ecx, %%eax"); //and eax to ecx, store in eax
		break;
	case op_mod_assign:
		gen_asm("xor %%edx, %%edx");
		gen_asm("cdq");
		gen_asm("idivl %%ecx");
		gen_asm("movl %%edx, %%eax");
		break;

	default:
		assert(false);
	}

	gen_annotate("store result");
	gen_asm("movl %%eax, (%%ebx)");

	if(lval_prev)
	{
		gen_annotate("leave lval address in eax");
		gen_asm("movl %%ebx, %%eax");
	}

	gen_annotate_end();
}

void gen_int_literal(ast_expression_t* expr)
{
	gen_annotate_start("int literal expression %d", expr->data.int_literal.val.v.int64);
	gen_asm("movl $%d, %%eax", int_val_as_uint32(&expr->data.int_literal.val));
	gen_annotate_end();
}

void gen_identifier(ast_expression_t* expr)
{
	gen_annotate_start("identifier expression '%s'", expr->data.identifier.name);

	if (expr->sema.result_type->kind == type_func_sig)
	{
		gen_annotate("'%s' is a function pointer, load its address in eax", expr->data.identifier.name);;
		gen_asm("movl $%s, %%eax", expr->data.identifier.name);
	}
	else
	{
		var_data_t* var = var_find(gen_var_set(), expr->data.identifier.name);
		assert(var);

		if (var->kind == var_global)
			gen_annotate("'%s' is global with label '%s'", expr->data.identifier.name, var->global_name);
		else
			gen_annotate("'%s' is local at %d(%%ebp)", expr->data.identifier.name, var->bsp_offset);

		//if we are processing an lval or we are referencing a user defined type load the address into eax
		if (lval || expr->sema.result_type->kind == type_user || ast_is_array_decl(var->var_decl))
		{
			if (lval)
				gen_annotate("loading address of lval into eax");
			else if (ast_is_array_decl(var->var_decl))
				gen_annotate("loading address of array into eax");
			else
				gen_annotate("loading address of user_type into eax");

			//place address in eax
			if (var->kind == var_global)
				gen_asm("leal %s, %%eax", var->global_name);
			else
				gen_asm("leal %d(%%ebp), %%eax", var->bsp_offset);
		}
		else
		{
			gen_annotate("loading value in eax");
			//place value in eax
			if (var->kind == var_global)
				gen_asm("movl %s, %%eax", var->global_name);
			else
				gen_asm("movl %d(%%ebp), %%eax", var->bsp_offset);
		}
	}

	gen_annotate_end();
}

void gen_logical_binary(ast_expression_t* expr)
{
	gen_annotate_start("logical binary expression %s", ast_op_name(expr->data.binary_op.operation));
	char label1[16];
	char label2[16];
	gen_make_label_name(label1);
	gen_make_label_name(label2);

	gen_annotate("lhs");
	gen_expression(expr->data.binary_op.lhs);

	if (expr->data.binary_op.operation == op_or)
	{
		gen_annotate("or");
		gen_asm("cmpl $0, %%eax"); //compare eax to 0
		gen_asm("je %s", label1);	//eax is the result of the first expression. If true we exit early
		gen_asm("movl $1, %%eax"); //set eax to 1 (result)
		gen_asm("jmp %s", label2);

		gen_annotate("rhs");
		gen_asm("%s:", label1);
		gen_expression(expr->data.binary_op.rhs);
		gen_asm("cmpl $0, %%eax"); //compare eax to 0
		gen_asm("movl $0, %%eax"); //set eax to 0
		gen_asm("setne %%al"); //test flags of comparison
		gen_asm("%s:", label2);
	}
	else if (expr->data.binary_op.operation == op_and)
	{
		gen_annotate("and");
		gen_asm("cmpl $0, %%eax"); //compare eax to 0
		gen_asm("jne %s", label1);
		//eax already 0 (the result)
		gen_asm("jmp %s", label2); //exit early

		gen_annotate("rhs");
		gen_asm("%s:", label1);
		gen_expression(expr->data.binary_op.rhs);
		gen_asm("cmpl $0, %%eax"); //compare eax to 0
		gen_asm("movl $0, %%eax"); //set eax to 0
		gen_asm("setne %%al"); //test flags of comparison
		gen_asm("%s:", label2);
	}
	gen_annotate_end();
}

void gen_arithmetic_binary(ast_expression_t* expr)
{
	gen_annotate_start("arithmetic binary expression %s", ast_op_name(expr->data.binary_op.operation));

	gen_annotate("rhs");
	gen_expression(expr->data.binary_op.rhs);
	gen_asm("push %%eax");

	gen_annotate("lhs");
	gen_expression(expr->data.binary_op.lhs);
	gen_asm("pop %%ecx");

	switch (expr->data.binary_op.operation)
	{
	case op_add:
		gen_asm("addl %%ecx, %%eax");
		break;
	case op_sub:
		gen_asm("subl %%ecx, %%eax");
		break;
	case op_mul:
		gen_asm("imul %%ecx, %%eax");
		break;
	case op_div:
		//clear edx
		gen_asm("xor %%edx, %%edx");
		//sign extend eax into edx:eax
		gen_asm("cdq");
		gen_asm("idivl %%ecx");
		break;
	case op_shiftleft:
		gen_asm("sall %%cl, %%eax");
		break;
	case op_shiftright:
		gen_asm("sarl %%cl, %%eax");
		break;
	case op_eq:
		gen_asm("cmpl %%ecx, %%eax"); //compare eax to ecx
		gen_asm("movl $0, %%eax"); //set eax to 0
		gen_asm("sete %%al"); //test flags of comparison
		break;
	case op_neq:
		gen_asm("cmpl %%ecx, %%eax"); //compare eax to ecx
		gen_asm("movl $0, %%eax"); //set eax to 0
		gen_asm("setne %%al"); //test flags of comparison
		break;
	case op_greaterthanequal:
		gen_asm("cmpl %%ecx, %%eax"); //compare eax to ecx
		gen_asm("movl $0, %%eax"); //set eax to 0
		gen_asm("setge %%al"); //test flags of comparison
		break;
	case op_greaterthan:
		gen_asm("cmpl %%ecx, %%eax"); //compare eax to ecx
		gen_asm("movl $0, %%eax"); //set eax to 0
		gen_asm("setg %%al"); //test flags of comparison
		break;
	case op_lessthanequal:
		gen_asm("cmpl %%ecx, %%eax"); //compare eax to ecx
		gen_asm("movl $0, %%eax"); //set eax to 0
		gen_asm("setle %%al"); //test flags of comparison
		break;
	case op_lessthan:
		gen_asm("cmpl %%ecx, %%eax"); //compare eax to ecx
		gen_asm("movl $0, %%eax"); //set eax to 0
		gen_asm("setl %%al"); //test flags of comparison
		break;
	case op_bitwise_and:
		gen_asm("andl %%ecx, %%eax"); //and eax to ecx, store in eax
		break;
	case op_bitwise_or:
		gen_asm("orl %%ecx, %%eax"); //and eax to ecx, store in eax
		break;
	case op_bitwise_xor:
		gen_asm("xorl %%ecx, %%eax"); //and eax to ecx, store in eax
		break;
	case op_mod:
		gen_asm("xor %%edx, %%edx");
		gen_asm("cdq");
		gen_asm("idivl %%ecx");
		gen_asm("movl %%edx, %%eax");
		break;
	}
	gen_annotate_end();
}

void gen_condition_expr(ast_expression_t* expr)
{
	gen_annotate_start("conditional expression");

	char label_false[16];
	char label_end[16];
	gen_make_label_name(label_false);
	gen_make_label_name(label_end);

	gen_annotate("condition");
	gen_expression(expr->data.condition.cond); //condition
	gen_asm("cmpl $0, %%eax"); //was false?
	gen_asm("je %s", label_false);
	
	gen_annotate("true branch");
	gen_expression(expr->data.condition.true_branch); //true
	gen_asm("jmp %s", label_end);
	gen_asm("%s:", label_false);
	
	gen_annotate("false branch");
	gen_expression(expr->data.condition.false_branch);
	gen_asm("%s:", label_end);
	gen_annotate_end();
}

void gen_member_access(ast_expression_t* expr)
{
	assert(expr->data.binary_op.lhs->sema.result_type->kind == type_user);
	ast_type_spec_t* user_type = expr->data.binary_op.lhs->sema.result_type;
	assert(expr->data.binary_op.rhs->kind == expr_identifier);
	ast_struct_member_t* member = ast_find_struct_member(user_type->data.user_type_spec,
		expr->data.binary_op.rhs->data.identifier.name);
	assert(member);

	gen_annotate_start("member access '%s'", member->decl->name);

	gen_annotate("lhs");
	gen_expression(expr->data.binary_op.lhs);
		
	//assume eax contains a pointer
	gen_annotate("add offset %d", member->offset);
	gen_asm("addl $%d, %%eax", member->offset);

	if (!lval && expr->sema.result_type->kind != type_user)
	{
		gen_annotate("store member value in eax");
		//we are not processing an lval and the member type is not a user type so move the value into eax
		gen_asm("movl (%%eax), %%eax");
	}
	gen_annotate_end();
}

void gen_ptr_member_access(ast_expression_t* expr)
{
	assert(expr->data.binary_op.lhs->sema.result_type->kind == type_ptr);
	ast_type_spec_t* user_type = expr->data.binary_op.lhs->sema.result_type->data.ptr_type;
	assert(user_type->kind == type_user);
	assert(expr->data.binary_op.rhs->kind == expr_identifier);

	ast_struct_member_t* member = ast_find_struct_member(user_type->data.user_type_spec,
		expr->data.binary_op.rhs->data.identifier.name);
	assert(member);

	gen_annotate_start("pointer member access '%s'", member->decl->name);

	gen_annotate("lhs");
	gen_expression(expr->data.binary_op.lhs);

	if (lval)
	{
		//we want to be left with the address of the member in eax
		gen_asm("movl (%%eax), %%eax");
	}
	
	//assume eax contains a pointer
	gen_annotate("add offset %d", member->offset);
	gen_asm("addl $%d, %%eax", member->offset);

	if (!lval && expr->sema.result_type->kind != type_user)
	{
		gen_annotate("store member value in eax");
		//we are not processing an lval and the member type is not a user type so move the value into eax
		gen_asm("movl (%%eax), %%eax");
	}
	gen_annotate_end();
}

void gen_array_subscript(ast_expression_t* expr)
{
	assert(expr->data.binary_op.lhs->sema.result_type->kind == type_ptr);

	gen_annotate_start("array subscript");

	//generate the lhs which should result in a pointer in eax
	//push the pointer
	//generate rhs (the index) which should result in an int offset
	//multiply this by the size of the item pointed to
	//pop the pointer from the stack and add the offset
	
	gen_annotate("lhs");

	gen_expression(expr->data.binary_op.lhs);
	gen_annotate("push lhs pointer");
	gen_asm("pushl %%eax");

	//index
	gen_annotate("rhs");
	gen_expression(expr->data.binary_op.rhs);
	gen_annotate("mul by size of item in array");
	gen_asm("imul $%d, %%eax", expr->sema.result_type->size);

	gen_annotate("pop the address of rhs from the stack and add the rhs * size offset in eax");
	gen_asm("popl %%ecx");
	gen_asm("addl %%ecx, %%eax");

	if (!lval && expr->sema.result_type->kind != type_user)
	{
		gen_annotate("store item value in eax");
		//we are not processing an lval and the member type is not a user type so move the value into eax
		gen_asm("movl (%%eax), %%eax");
	}

	gen_annotate_end();
}

void gen_address_of(ast_expression_t* expr)
{
	gen_annotate_start("address of");
	bool p = lval;
	lval = true;
	gen_expression(expr->data.unary_op.expression);
	lval = p;
	gen_annotate_end();
}

void gen_dereference(ast_expression_t* expr)
{
	gen_annotate_start("dereference");
	gen_expression(expr->data.unary_op.expression);

	if (lval || expr->sema.result_type->kind != type_user)
	{
		gen_asm("movl (%%eax), %%eax");
	}
	gen_annotate_end();
}

void gen_func_call(ast_expression_t* expr)
{
	gen_annotate_start("funcation call");

	//find the sig of the function we're calling
	ast_type_spec_t* target_fn_type = ast_type_is_fn_ptr(expr->data.func_call.sema.target_type) ?
		expr->data.func_call.sema.target_type->data.ptr_type : expr->data.func_call.sema.target_type;
	assert(target_fn_type->kind == type_func_sig);
	ast_func_sig_type_spec_t* sig = target_fn_type->data.func_sig_spec;
	assert(sig);

	uint32_t pushed = 0;
	if (sig->ret_type->size > 4)
	{
		gen_annotate("allocate space for return value > 4 bytes in length");
		//allocate space for return value on the stack
		gen_asm("subl $%d, %%esp", sig->ret_type->size);
		gen_asm("movl %%esp, %%ebx"); //store the return ptr
		pushed = sig->ret_type->size; //cleanup the stack space after the call
	}

	ast_func_call_param_t* param = expr->data.func_call.last_param;
	if(param)
		gen_annotate("push parameters");
	else
		gen_annotate("no parameters");

	while (param)
	{
		gen_expression(param->expr);
		ast_type_spec_t* param_type = param->expr->sema.result_type;

		if (param_type->size == 1)
		{
			gen_asm("%s %%al, %%edx", _promoting_mov_instr(param_type));
			gen_asm("pushl %%edx");
			pushed += 4;
		}
		else if (param_type->size == 2)
		{
			gen_asm("%s %%ax, %%edx", _promoting_mov_instr(param_type));
			gen_asm("pushl %%edx");
			pushed += 4;
		}
		else if (param_type->size == 4)
		{
			gen_asm("pushl %%eax");
			pushed += 4;
		}
		else
		{
			//address is in eax

			uint32_t offset = param_type->size - 4;
			size_t sz = param_type->size;
			while (sz > 0)
			{
				gen_asm("pushl %d(%%eax)", offset);
				offset -= 4;
				sz -= 4;
			}
			pushed += param_type->size;
		}

		param = param->prev;
	}

	if (sig->ret_type->size > 4)
	{
		gen_annotate("push address of space allocated for return value");
		gen_asm("push %%ebx");		
	}

	//are we calling a function directly by name, or via a pointer?
	if (ast_type_is_fn_ptr(expr->data.func_call.sema.target_type))
	{
		gen_annotate("function pointer target");
		gen_expression(expr->data.func_call.target);
		gen_annotate("call");
		gen_asm("call *%%eax");
	}
	else
	{
		//if calling directly the target must be an identifier
		assert(expr->data.func_call.target->kind == expr_identifier);
		gen_annotate("call");
		gen_asm("call %s", expr->data.func_call.target->data.identifier.name);
	}

	if (pushed > 0)
	{
		gen_annotate("release %d bytes of stack space", pushed);
		gen_asm("addl $%d, %%esp", pushed);
	}
	gen_annotate_end();
}

void gen_str_literal(ast_expression_t* expr)
{
	gen_annotate_start("expr_str_literal '%s'", expr->data.str_literal.value);
	gen_asm("leal .%s, %%eax", expr->data.str_literal.label);
	gen_annotate_end();
}

void gen_logical_unary(ast_expression_t* expr)
{
	ast_expression_t* param = expr->data.unary_op.expression;
	switch (expr->data.unary_op.operation)
	{
	case op_negate:
		gen_expression(param);
		gen_asm("neg %%eax");
		break;
	case op_compliment:
		gen_expression(param);
		gen_asm("not %%eax");
		break;
	case op_not:
		gen_expression(param);
		gen_asm("cmpl $0, %%eax"); //compare eax to 0
		gen_asm("movl $0, %%eax"); //set eax to 0
		gen_asm("sete %%al"); //if eax was 0 in cmpl, set al to 1
		break;
	default:
		assert(false);
	}
}

void gen_prefix_unary(ast_expression_t* expr)
{
	gen_annotate_start("prefix op %s", ast_op_name(expr->data.unary_op.operation));
	
	bool prev_lval = lval;
	lval = true;
	gen_expression(expr->data.unary_op.expression);
	lval = prev_lval;

	gen_annotate("store target address");
	gen_asm("movl %%eax, %%edx");
	gen_annotate("load value into eax");
	gen_asm("movl (%%eax), %%eax");
	if (expr->data.unary_op.operation == op_prefix_inc)
		gen_asm("incl %%eax");
	else
		gen_asm("decl %%eax");

	gen_annotate("store result");
	gen_asm("movl %%eax, (%%edx)");

	if (prev_lval)
	{
		gen_asm("movl %%edx, %%eax");
	}
	
	gen_annotate_end();
}


void gen_postfix_op(ast_expression_t* expr)
{
	gen_annotate_start("postfix op %s", ast_op_name(expr->data.unary_op.operation));

	bool prev_lval = lval;
	lval = true;
	gen_annotate("param");
	gen_expression(expr->data.unary_op.expression);
	lval = prev_lval;

	gen_annotate("store target address");
	gen_asm("movl %%eax, %%edx");
	gen_annotate("load value into eax");
	gen_asm("movl (%%eax), %%eax");
	
	if (!prev_lval)
	{
		gen_annotate("store unincremented value");
		gen_asm("pushl %%eax");
	}	

	if (expr->data.unary_op.operation == op_postfix_inc)
		gen_asm("incl %%eax");
	else
		gen_asm("decl %%eax");

	gen_annotate("store result");
	gen_asm("movl %%eax, (%%edx)");

	if (prev_lval)
	{
		gen_annotate("restore lval address");
		gen_asm("movl %%edx, %%eax");
	}
	else
	{
		gen_annotate("restore unincremented value");
		gen_asm("popl %%eax");
	}

	gen_annotate_end();
}

void gen_binary_op(ast_expression_t* expr)
{
	if (is_binary_op(expr, op_assign))
		gen_assignment(expr);
	else if (ast_is_assignment_op(expr->data.binary_op.operation))
		gen_op_assign(expr);
	else if (is_binary_op(expr, op_and) || is_binary_op(expr, op_or))
		gen_logical_binary(expr);
	else if (is_binary_op(expr, op_member_access))
		gen_member_access(expr);
	else if (is_binary_op(expr, op_ptr_member_access))
		gen_ptr_member_access(expr);
	else if (is_binary_op(expr, op_array_subscript))
		gen_array_subscript(expr);
	else
		gen_arithmetic_binary(expr);
}

void gen_unary_op(ast_expression_t* expr)
{
	if (is_unary_op(expr, op_address_of))
		gen_address_of(expr);
	else if (is_unary_op(expr, op_dereference))
		gen_dereference(expr);
	else if (is_unary_op(expr, op_negate) || is_unary_op(expr, op_compliment) || is_unary_op(expr, op_not))
		gen_logical_unary(expr);
	else if (is_unary_op(expr, op_prefix_inc) || is_unary_op(expr, op_prefix_dec))
		gen_prefix_unary(expr);
	else if (is_unary_op(expr, op_postfix_inc) || is_unary_op(expr, op_postfix_dec))
		gen_postfix_op(expr);
	else
		assert(false);
}

/*
Generate code for expr

The value left in EAX will depend on the expression result type (expr->sema.result_type)
	-	for any pointer type the address will be placed in EAX
	-	for user types greater than 4 bytes in length the address will be placed in EAX
	-	for types <= 4 bytes the value will be placed in EAX

*/
void gen_expression(ast_expression_t* expr)
{
	if (expr->kind == expr_identifier)
		gen_identifier(expr);
	else if (expr->kind == expr_int_literal)
		gen_int_literal(expr);
	else if (expr->kind == expr_condition)
		gen_condition_expr(expr);
	else if (expr->kind == expr_func_call)
		gen_func_call(expr);
	else if (expr->kind == expr_str_literal)
		gen_str_literal(expr);
	else if (expr->kind == expr_unary_op)
		gen_unary_op(expr);
	else if (expr->kind == expr_binary_op)
		gen_binary_op(expr);
	else if(expr->kind != expr_null)
		assert(false);
}
