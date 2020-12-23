
int main()
{
	typedef struct  
	{
		int a; int b;
	} A;
	A a;
	a.a = 5;
	a.b = 1;

	A a2 = a;
	A a3;
	
	a3 = a2;

	return a3.a + a3.b;
}

