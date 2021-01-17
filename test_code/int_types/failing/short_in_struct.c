struct A{ int i; short c; int j;};

int main()
{
	struct A a;
	a.i = 1000;
	a.c = 50000;
	a.j = 50;

	return a.j;
}
