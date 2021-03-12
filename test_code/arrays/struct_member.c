
typedef struct
{
	int mem[10];
}type_t;


int main()
{
	type_t t;

	t.mem[5] = 12;
	return t.mem[5];
}
