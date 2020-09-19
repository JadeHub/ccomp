struct A {int a; int b;};

int foo(struct A a, struct A b)
{
	return a.a / b.b;
}

int main()
{
	struct A a;
	a.a = 50;
	a.b = 2;

	struct A b = a;
	b = a;

	return foo(a, b);
}
