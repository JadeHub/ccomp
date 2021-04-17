typedef struct
{
	int vals[10];
}t_a;

typedef struct
{
	t_a ts[4];
}t_b;

int main()
{
	t_a a1;
	t_a a2;

	t_b b1;
	t_b b2;

	for(int i=0;i<10;i++)
		a1.vals[i] = 100 + i;

	a2 = a1;

	b1.ts[2] = a2;
	b2 = b1;
	
	return b2.ts[2].vals[7];
}
