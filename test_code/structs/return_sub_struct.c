struct B {int a; int b;};
struct A {struct B ab; int i;};

int main()
{
	struct A a;

	a.ab.a = 1;
	a.ab.b = 10;
	a.i = 100;

	return a.ab.b;
}
