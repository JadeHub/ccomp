struct Sub { int x; int y; int z; };
struct A { int a; struct Sub* s; int b; };

int main()
{
	struct A aVal;
	aVal.a = 5;
	aVal.b = 7;
	
	return aVal.a + ++aVal.b;
}

