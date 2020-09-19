struct A {int a; int b;};

int foo(struct A c)
{
	return c.a / c.b;
}

int main()
{
	struct A a;
	a.a = 50;
	a.b = 2;

	return foo(a);
}
