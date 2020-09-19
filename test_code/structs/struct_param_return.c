
struct A
{
	int a; int b;
};

struct A foo(int i)
{
	struct A a;

	a.a = i;
	a.b = 5;
	return a;
}

int main()
{
	struct A b;
	
	b = foo(4);

	return b.a + b.b;
}

