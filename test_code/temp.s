.globl main
main:
push %ebp
movl %esp, %ebp
movl $2, %eax
push %eax
movl $2, %eax
pop %ecx
addl %ecx, %eax
movl $0, %eax
leave
ret
