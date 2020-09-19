struct A {int a; int b;};

int main()
{
	struct A i;
	i.a = 5;
	i.b = 7;
		
	struct A* ptr = &i;
	struct A** pp = &ptr;

	(**pp).b = 11;

	return i.a + i.b;
}
