typedef struct {int i; int j; } A;

A fn(int i, int j)
{
	A a;
	a.i = i;
	a.j = j;
	return a;
}

int main()
{
	return fn(10, 20).j;
}
