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

int main()
{
	struct A f;
 	int i;
	i = (f = boo()).a;
	return i;
}

