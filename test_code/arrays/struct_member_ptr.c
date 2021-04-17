
typedef struct
{
	int p;
	int mem[10];
}type_t;


int main()
{
	type_t t;
	type_t* tp = &t;

	int* p = tp->mem;

	*p = 5;
	return *p;
}
