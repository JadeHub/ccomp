
int main()
{
	struct A {int a; int b;} v;
	v.a = 5;
	v.b = 1;

	return v.b + v.a;
}

