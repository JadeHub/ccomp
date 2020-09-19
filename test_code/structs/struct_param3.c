
struct A
{
	int a; int b;
};

int foo(struct A a, struct A b)
{
	return a.a + b.b;
}

int main()
{
	struct A a;
	
	a.a = 1;
	a.b = 0;

	struct A b;

	b.a = 0;
	b.b = 5;
	
	return foo(a, b);
}

