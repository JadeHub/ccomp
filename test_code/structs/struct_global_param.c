struct A {int a; int b;} a;

int foo(struct A c)
{
	return c.a / c.b;
}

int main()
{
	a.a = 50;
	a.b = 2;

	return foo(a);
}
