struct A {char c; int a; short s; } a = {1, 2, 30000};

int main()
{
	return a.a + a.c + a.s;
}

