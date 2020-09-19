
struct A
{
	int a; int b;
};

struct A foo()
{
	struct A a;

	a.a = 5;
	a.b = 1;

	return a;
}

struct A b;

int main()
{
	b = foo();

	return b.a + b.b;
}

