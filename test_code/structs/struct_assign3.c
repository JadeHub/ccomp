
int main()
{
	struct A 
	{
		int a; int b;
	} a;
	a.a = 5;
	a.b = 1;

	struct A a2 = a;
	struct A a3;
	
	a3 = a2;

	return a3.a + a3.b;
}

