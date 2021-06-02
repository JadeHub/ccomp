#!/bin/bash

comp()
{
	echo $1
	../build/compiler/jcc -c comp.cfg ../libcomp/src/$1 > asm/$1
}

comp abi.c
comp ast.c
comp code_gen.c
comp code_gen_expr.c
comp comp_opt.c
comp diag.c
comp id_map.c
comp id_map.c
comp int_val.c 
comp lexer.c
comp parse.c
comp parse_decl.c
comp parse_smnt.c
comp parse_type.c
comp pp_built_in_defs.c