struct A {int a; int b;};

int main()
{
	struct A a;
	a.a = 5;
	a.b = 7;

	struct A *ptr = &a;

	struct A b;
	b.a = 50;
	b.b = 70;

	*ptr = b;
	
	return a.a + a.b;
}

