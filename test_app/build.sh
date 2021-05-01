rm -f temp/*
rm test

#c to asm
./jcc.sh main.c > temp/main.s
./jcc.sh test.c > temp/test.s

#asm to obj files
gcc -m32 temp/main.s -c -o temp/main.o
gcc -m32 temp/test.s -c -o temp/test.o

#link objs to exe
gcc -m32 temp/test.o temp/main.o -o test
