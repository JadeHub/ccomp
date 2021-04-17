typedef struct
{
	int vals[10];
}t_1;

typedef struct
{
	t_1* ptrs[4];
}t_2;

int main()
{
	t_1 a1;
	t_1 a2;

	t_2 t2;

	t2.ptrs[0] = &a1;
	t2.ptrs[1] = &a2;

	t2.ptrs[0]->vals[2] = 7;
	t2.ptrs[1]->vals[0] = 3;

	return a1.vals[2] + a2.vals[0];
}
