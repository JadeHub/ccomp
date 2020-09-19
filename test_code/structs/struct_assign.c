
int main()
{
	struct A 
	{
		int a; int b;
	} a;
	a.a = 5;
	a.b = 1;

	struct A a2;
	
	a2 = a;

	return a2.a + a2.b;
}

