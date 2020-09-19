struct A
{
	int a; int b;
};

struct A boo()
{
	struct A a;
	a.a = 5;
	a.b = 2;

	return a;
}

struct A boo2()
{
	struct A f;
	return f = boo();
}

int main()
{
	struct A a;
	a = boo2();
	return a.a + a.b;
}

