
struct A
{
	int a; int b;
};

struct A foo(int i)
{
	struct A a;

	a.a = 5;
	a.b = i;

	return a;
}

int main()
{
	struct A b;
	b = foo(5);
	foo(12);

	return b.a + b.b;
}

