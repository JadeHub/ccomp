struct A { int a; int b; } g;

struct A* fn()
{
	g.a = 1;
	g.b = 5;
	return &g;
}


int main()
{
	struct A* a;
	
	(a = fn())->b = 20;
//	a = fn();


	return a->a + a->b;
}

