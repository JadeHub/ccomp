
int main()
{
	struct A {int a; int b;} a;
	a.a = 5;
	int x = 7;
	a.b = x;

	return a.a;
}

