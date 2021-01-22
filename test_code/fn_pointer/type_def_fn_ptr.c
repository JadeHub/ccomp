int fn(int a, char* f)
{
	return a + f[0];
}

typedef int (*fn_t)(int, char*);

int main()
{
	fn_t p;
//	int (*p)(int, char*);
	p = fn;
	return p(1, "Hello");	
}
