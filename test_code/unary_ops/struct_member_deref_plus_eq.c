typedef struct
{
	int* i;
}A;

int main()
{
	A a;
	int q = 5;
	A* p = &a;
	a.i = &q;
	*p->i += 10;
	return *a.i;
}
