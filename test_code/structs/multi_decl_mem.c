typedef struct
{
	int a, b, *c;
}type_t;

int main()
{
	type_t t;
	t.a = 5;
	t.b = 10;
	t.c = &t.b;

	return t.a + t.b + *t.c;
}
