typedef struct
{
	int i;
}A;

int main()
{
	A a;
	A* p = &a;
	a.i = 5;
	p->i += 10;
	return a.i;
}
