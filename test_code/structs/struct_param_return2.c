
struct A
{
	int a; int b;
};

struct A foo(struct A a)
{
	a.a = 1;
	a.b = 5;
	return a;
}

int main()
{
	struct A b;
	
	b = foo(b);

	return b.a + b.b;
}

