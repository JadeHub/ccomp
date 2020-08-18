#include "code_gen.h"
#include "var_set.h"
#include "source.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>


void gen_expression(ast_expression_t* expr);
void gen_block_item(ast_block_item_t* bi);
ast_type_spec_t* gen_func_call_expression(ast_expression_t* expr);
void gen_assignment_expression(ast_expression_t* expr);

typedef struct
{
	char name[128]; //set if global var
	ast_type_spec_t* type; //target value type
	int32_t stack_offset;

	bool push;
	uint32_t deref;
}lval_data_t;


static write_asm_cb _asm_cb;
static void* _asm_cb_data;

static var_set_t* _var_set;
static bool _returned = false;
static ast_function_decl_t* _cur_fun = NULL;
static valid_trans_unit_t* _cur_tl = NULL;

static const char* _cur_break_label = NULL;
static const char* _cur_cont_label = NULL;

static void _gen_asm(const char* format, ...)
{
	char buff[256];

	va_list args;
	va_start(args, format);
	vsnprintf(buff, 256, format, args);
	va_end(args);

	_asm_cb(buff, _asm_cb_data);
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
	else if (target->kind == expr_unary_op &&
		target->data.unary_op.operation == op_dereference)
	{
 		result = _get_lvalue_addr(target->data.unary_op.expression);
		result.deref++;
		return result;
	}
	else
	{
		assert(false);
	}
	return result;
}

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

ast_declaration_t* _find_func_decl(const char* name)
{
	ast_declaration_t* decl = _cur_tl->functions;

	while (decl)
	{
		if (strcmp(name, decl->data.func.name) == 0)
			return decl;
		decl = decl->next;
	}
	return NULL;
}

typedef struct
{
	ast_type_spec_t* type;

	enum
	{
		lval_none,
		lval_stack,		//at stack_offset(%ebp) + offset
		lval_address,	//address + offset
		lval_label,		//global + offset
		lval_push		//push to stack
	}kind;

	union
	{
		char* label;
		int32_t stack_offset;
	};

	uint32_t offset;
	bool done;
}lval_t;


typedef struct
{
	ast_type_spec_t* type;
	lval_t lval;
}expr_result;

lval_t* _cur_lval = NULL;

void gen_assignment_expression_impl(ast_expression_t* expr, lval_t* result); //tdo

static inline bool _is_binary_op(ast_expression_t* expr, op_kind op)
{
	return expr->kind == expr_binary_op && expr->data.binary_op.operation == op;
}

static inline bool _is_unary_op(ast_expression_t* expr, op_kind op)
{
	return expr->kind == expr_unary_op && expr->data.unary_op.operation == op;
}

void gen_lval_target_expr(ast_expression_t* expr, lval_t* result)
{
	if (expr->kind == expr_identifier)
	{
		var_data_t* var = var_find(_var_set, expr->data.var_reference.name);
		if (!var)
		{
			diag_err(expr->tokens.start, ERR_UNKNOWN_VAR,
				"reference to unknown variabe %s",
				expr->data.var_reference.name);
			return ;
		}

		result->type = var->data.decl->type;

		if (var->kind == var_global)
		{
			result->kind = lval_label;
			result->label = var->global_name;

			//load the value, or the address of the value if greater than 4 bytes
			if(result->type->size <= 4)
				_gen_asm("%s %s, %%eax", _promoting_mov_instr(result->type), result->label);
			else
				_gen_asm("leal %s, %%eax", result->label);
		}
		else
		{
			result->kind = lval_stack;
			result->stack_offset = var->bsp_offset;

			if (result->type->size <= 4)
				_gen_asm("%s %d(%%ebp), %%eax", _promoting_mov_instr(result->type), result->stack_offset);
			else
				_gen_asm("leal %d(%%ebp), %%eax", result->stack_offset);
		}
	}
	else if (expr->kind == expr_func_call)
	{
		ast_type_spec_t* ret_type =  gen_func_call_expression(expr);
		result->type = ret_type;
	}
	else if (_is_binary_op(expr, op_member_access))
	{
//		assert(result->kind == lval_address);
		//lhs.rhs
		gen_lval_target_expr(expr->data.binary_op.lhs, result);

		assert(result->type);
		//adjust result to indicate the member of lhs
		assert(expr->data.binary_op.rhs->kind == expr_identifier);

		ast_struct_member_t* member = ast_find_struct_member(result->type->user_type_spec,
									expr->data.binary_op.rhs->data.var_reference.name);
		assert(member); 
		result->offset += member->offset;
		result->type = member->type;
	}
	else if (_is_binary_op(expr, op_ptr_member_access))
	{
		//		assert(result->kind == lval_address);
				//lhs.rhs
		gen_lval_target_expr(expr->data.binary_op.lhs, result);

		/* identical to op_dereference below */
		//move value to eax
		switch (result->kind)
		{
		case lval_label:
			_gen_asm("movl %s, %%eax", result->label);
			result->kind = lval_address;
			break;
		case lval_stack:
			_gen_asm("movl %d(%%ebp), %%eax", result->stack_offset);
			result->kind = lval_address;
			break;
		case lval_address:
			//already an address in eax					
			break;
		}

		if (result->type->ptr_type->kind == type_ptr)
		{
			_gen_asm("movl (%%eax), %%eax", result->stack_offset);
		}

		assert(result->type);
		assert(result->type->kind == type_ptr);
		assert(result->type->ptr_type->kind == type_user);
		//adjust result to indicate the member of lhs
		assert(expr->data.binary_op.rhs->kind == expr_identifier);

		ast_struct_member_t* member = ast_find_struct_member(result->type->ptr_type->user_type_spec,
			expr->data.binary_op.rhs->data.var_reference.name);
		assert(member);
		result->offset += member->offset;
		result->type = member->type;
	}
	else if (_is_unary_op(expr, op_address_of))
	{
		gen_lval_target_expr(expr->data.unary_op.expression, result);

		//generate code to place the target address in eax
		//adjust result type

		assert(result->kind != lval_none);
		switch (result->kind)
		{
		case lval_label:
			_gen_asm("leal %s, %%eax", result->label);
			result->kind = lval_address;
			break;		
		case lval_stack:
			_gen_asm("leal %d(%%ebp), %%eax", result->stack_offset);
			result->kind = lval_address;
			break;
		case lval_address:
			assert(false);
			//_gen_asm("leal (%%eax), %%eax");
			break;
		}
		result->type = ast_make_ptr_type(result->type);
	}
	else if (_is_unary_op(expr, op_dereference))
	{
		gen_lval_target_expr(expr->data.unary_op.expression, result);

		//must be a pointer type to be dereferenced
		assert(result->kind != lval_none);
		assert(result->type->kind == type_ptr);
				
		//move value to eax
		switch (result->kind)
		{
		case lval_label:
			_gen_asm("movl %s, %%eax", result->label);
			result->kind = lval_address;
			break;
		case lval_stack:
			_gen_asm("movl %d(%%ebp), %%eax", result->stack_offset);
			result->kind = lval_address;
			break;
		case lval_address:
			//already an address in eax					
			break;
		}

		if (result->type->ptr_type->kind == type_ptr)
		{
			_gen_asm("movl (%%eax), %%eax", result->stack_offset);
		}
		
		//adjust type
		result->type = result->type->ptr_type;
		result->kind = lval_address;
	}
	else if (expr->kind == expr_assign)
	{
		gen_assignment_expression_impl(expr, result);
	}
}

void gen_copy_eax_to_lval(lval_t* lval)
{
	/*
	* eax contains the source
	* lval specifies the destination 
	* 
	* for types <= 4 byte length the desination can be a stack offset (+ offset), label (+ offset), or a memory address stored in edx (+ offset)
	* for types > 4 byte length, eax always represents an address and data is copied from there to the address at edx, or an address stored on the stack
	* 
	*/
	if (lval->type->size <= 4)
	{
		//copy from eax to the destination specified in lval
		switch (lval->kind)
		{
		case lval_stack:
			_gen_asm("movl %%eax, %d(%%ebp)", lval->stack_offset + lval->offset);
			break;
		case lval_label:
			if (lval->offset)
				_gen_asm("movl %%eax, %s + %d", lval->label, lval->offset);
			else
				_gen_asm("movl %%eax, %s", lval->label);
			break;
		case lval_address:
			if (lval->offset)
				_gen_asm("movl %%eax, %d(%%edx)", lval->offset);
			else
				_gen_asm("movl %%eax, (%%edx)");
			break;
		}
	}
	else
	{
		if (lval->kind == lval_stack)
		{
			// destination is an address which was stored on the stack			
			_gen_asm("leal %d(%%ebp), %%edx", lval->stack_offset);
		}
		else
		{
			assert(lval->kind == lval_address);
		}
		//source address is in eax, dest in edx
		int32_t dest_off = lval->stack_offset; // += lval->offset?
		uint32_t offset = 0;
		while (offset < lval->type->size)
		{
			_gen_asm("movl %d(%%eax), %%edx", offset);
			_gen_asm("movl %%edx, %d(%%ebp)", dest_off);
			offset += 4;
			dest_off += 4;
		}
	}
}

void gen_assignment_expression(ast_expression_t* expr)
{
	lval_t lval;
	memset(&lval, 0, sizeof(lval_t));
	lval.kind = lval_none;

	lval_t* prev_target = _cur_lval;
	_cur_lval = &lval;
	gen_assignment_expression_impl(expr, &lval);

	_cur_lval = prev_target;
}

void gen_assignment_expression_impl(ast_expression_t* expr, lval_t* lval)
{
	/*
	1) Process the target expression generating code to calculate the assignment target which can be one of:
		A) types of 4 bytes or less to be written to a stack location or address (all int and pointer types)
		B) types greater than 4 bytes in length are copied to a stack location or to an address stored in a stack location
	*/
	
	gen_lval_target_expr(expr->data.assignment.target, lval);
		
	assert(lval->kind != lval_none);

	if(lval->kind == lval_address)
		_gen_asm("push %%eax");

	gen_expression(expr->data.assignment.expr);

	if (!lval->done)
	{
		if (lval->kind == lval_address)
			_gen_asm("pop %%edx");
		gen_copy_eax_to_lval(lval);
	}
}

ast_type_spec_t* gen_func_call_expression(ast_expression_t* expr)
{
	ast_declaration_t* decl = _find_func_decl(expr->data.func_call.name);
	assert(decl);
	if (!decl)
		return NULL;

	ast_expression_t* param = expr->data.func_call.params;
	ast_declaration_t* param_decl = decl->data.func.params;
	uint32_t pushed = 0;

	if (decl->data.func.return_type->size > 4)
	{
		if (_cur_lval && _cur_lval->kind == lval_address)
		{
			//current lval address is on the stack, we need to pop it off and then push it after pushing params
			_gen_asm("pop %%ebx");
		}	
	}

	while (param)
	{
		gen_expression(param);

		if (param_decl->data.var.type->size <= 4)
		{

			_gen_asm("pushl %%eax");
			pushed += 4; //todo < 4?
		}
		else
		{
			//address is in eax

			uint32_t offset = param_decl->data.var.type->size - 4;
			size_t sz = param_decl->data.var.type->size;
			while (sz > 0)
			{
				_gen_asm("pushl %d(%%eax)", offset);
				offset -= 4;
				sz -= 4;
			}
			pushed += param_decl->data.var.type->size;
		}

		param = param->next;
		param_decl = param_decl->next;
	}

	if (decl->data.func.return_type->size > 4)
	{
		if (_cur_lval)
		{
			switch (_cur_lval->kind)
			{
			case lval_label:
				_gen_asm("leal %s, %%ebx", _cur_lval->label);
				break;
			case lval_stack:
				_gen_asm("leal %d(%%ebp), %%ebx", _cur_lval->stack_offset);
				break;
			case lval_address:
				break;
			}

			_gen_asm("push %%ebx");		
			_cur_lval->done = true;
		}
		else
		{
			//allocate space for return value on the stack
			_gen_asm("subl $%d, %%esp", decl->data.func.return_type->size);
			_gen_asm("push %%esp");
			pushed += decl->data.func.return_type->size;
		}
	}
	_gen_asm("call %s", expr->data.func_call.name);
	if (pushed > 0)
	{
		_gen_asm("addl $%d, %%esp", pushed);
	}
	return decl->data.func.return_type;
}

/*void gen_expression(ast_expression_t* expr)
{

}*/

void gen_expression(ast_expression_t* expr) 
{
	if (expr->kind == expr_null)
	{
		//null
		return;
	}

	_gen_asm("");

	if (expr->kind == expr_func_call)
	{
		gen_func_call_expression(expr);
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
		_gen_asm("# pos %s",src_file_pos_str(src_file_position(expr->tokens.start->loc)));
		gen_assignment_expression(expr);
	}
	else if (expr->kind == expr_identifier)
	{
		/*
		Variable reference		
		*/

		lval_t result;

		gen_lval_target_expr(expr, &result);

		//move value into eax
		switch (result.kind)
		{
			case lval_stack:
				if(result.type->size > 4)
					_gen_asm("leal %d(%%ebp), %%eax", result.stack_offset);
				else
					_gen_asm("%s %d(%%ebp), %%eax", _promoting_mov_instr(result.type), result.stack_offset);
				break;
			case lval_label:
				if (result.type->size > 4)
					_gen_asm("leal %s, %%eax", result.label);
				else
					_gen_asm("%s %s, %%eax", _promoting_mov_instr(result.type), result.label);
				break;
			case lval_address:
				break;
		}


	}
	else if (expr->kind == expr_int_literal)
	{
		_gen_asm("movl $%d, %%eax", expr->data.int_literal.value);
	}
	else if (expr->kind == expr_unary_op)
	{
		if (expr->data.unary_op.operation == op_address_of)
		{
			lval_t lval;
			memset(&lval, 0, sizeof(lval_t));
			gen_lval_target_expr(expr->data.unary_op.expression, &lval);

			//todo lval.offset

			switch (lval.kind)
			{
			case lval_label:
				_gen_asm("leal %s, %%eax", lval.label);
				
				break;
			case lval_stack:
				_gen_asm("leal %d(%%ebp), %%eax", lval.stack_offset);
				break;
			case lval_address:
				//assert(false); //? should be a lvalue
				_gen_asm("movl %%edx, %%eax");
				break;
			}

			return;
		}
		

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
		{
			lval_t lval;
			memset(&lval, 0, sizeof(lval_t));
			lval.kind = lval_none;

			gen_lval_target_expr(param, &lval);

			_gen_asm("incl %%eax");

			if (!lval.done)
			{
				gen_copy_eax_to_lval(&lval);
			}
			break;
		}
		case op_prefix_dec:
		{
			lval_t lval;
			memset(&lval, 0, sizeof(lval_t));
			lval.kind = lval_none;

			gen_lval_target_expr(param, &lval);

			_gen_asm("decl %%eax");

			if (!lval.done)
			{
				gen_copy_eax_to_lval(&lval);
			}
			break;
		}
		case op_dereference:
			_gen_asm("movl (%%eax), %%eax");
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
		{
			//get the value of the parameter
			lval_t lval;
			memset(&lval, 0, sizeof(lval_t));
			lval.kind = lval_none;
			gen_lval_target_expr(param, &lval);

			//save the current value
			_gen_asm("pushl %%eax");

			//increment
			_gen_asm("incl %%eax");

			if (!lval.done)
			{
				gen_copy_eax_to_lval(&lval);
			}

			//pop the un-incremented value back into eax
			_gen_asm("popl %%eax");

			break;
		}
		case op_postfix_dec:
		{
			//get the value of the parameter
			lval_t lval;
			memset(&lval, 0, sizeof(lval_t));
			lval.kind = lval_none;
			gen_lval_target_expr(param, &lval);

			//save the current value
			_gen_asm("pushl %%eax");

			//increment
			_gen_asm("decl %%eax");

			if (!lval.done)
			{
				gen_copy_eax_to_lval(&lval);
			}

			//pop the un-incremented value back into eax
			_gen_asm("popl %%eax");

			break;
		}
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

			if (strlen(lval.name))
			{
				if (lval.stack_offset)
				{
					_gen_asm("movl %s+%d, %%eax", lval.name, lval.stack_offset);
				}
				else
				{
					_gen_asm("movl %s, %%eax", lval.name);
				}
			}
			else
			{
				_gen_asm("movl %d(%%ebp), %%eax", lval.stack_offset);
			}
			/*
			lval_t result;

			gen_lval_target_expr(expr->data.binary_op.lhs, &result);

			assert(result.type);
			//adjust result to indicate the member of lhs
			assert(expr->data.binary_op.rhs->kind == expr_identifier);

			ast_struct_member_t* member = ast_find_struct_member(result.type->user_type_spec,
				expr->data.binary_op.rhs->data.var_reference.name);
			assert(member);

			_gen_asm("addl $%d, %%eax", member->offset);

			//result->offset += member->offset;
			//result->type = member->type;
			*/
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
		lval_t lval;
		memset(&lval, 0, sizeof(lval_t));
		lval.kind = lval_stack;
		lval.stack_offset = var->bsp_offset;
		lval.type = var_decl->type;

		gen_expression(var_decl->expr);

		if (!lval.done)
		{
			gen_copy_eax_to_lval(&lval);
		}
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
		gen_expression(smnt->data.expr);

		//address of result should be in eax, copy to the return address stored at 8(bsp)
		
		//the destination address is in the stack at stack_offset
		//load the address into edx
		_gen_asm("movl %d(%%ebp), %%edx", 8);
		
		//source address is in eax, dest in edx
		uint32_t offset = 0;
		while (offset < _cur_fun->return_type->size)
		{
			if (offset)
			{
				_gen_asm("movl %d(%%eax), %%ecx", offset);
				_gen_asm("movl %%ecx, %d(%%edx)", offset);
			}
			else
			{
				_gen_asm("movl (%%eax), %%ecx");
				_gen_asm("movl %%ecx, (%%edx)");
			}
			offset += 4;
		}
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
		//todo?	
		_gen_asm(".long %d", var->expr->data.int_literal.value); //data and init value
	}
	else
	{
		//0 or uninitialised var goes in .BSS section
		_gen_asm(".bss"); //bss section
		_gen_asm(".align 4");
		_gen_asm("_var_%s:", var->name); //label
		_gen_asm(".zero %d", var->type->size); //data length		
	}
	_gen_asm(".text"); //back to text section
	_gen_asm("\n");
}

void code_gen(valid_trans_unit_t* tl, write_asm_cb cb, void* data)
{
	_asm_cb = cb;
	_asm_cb_data = data;
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
	_asm_cb_data = NULL;
	_asm_cb = NULL;
}