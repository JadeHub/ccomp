
typedef struct
{
	int p;
	int mem[10];
}type_t;


int main()
{
	type_t t;

	int* p = t.mem;

	*p = 5;
	return *p;
}
