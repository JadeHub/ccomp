struct A{ int i; char c; int j;};

int main()
{
	struct A a;
	a.i = 1000;
	a.c = 100;
	a.j = 50;

	return a.j;
}
