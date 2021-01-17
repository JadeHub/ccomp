int f(int a, char* f)
{
	return a + f[0];
}

typedef int (*fn_ptr)(int, char*);

int main()
{
	fn_ptr p = f;
	return p(1, "Hello");	
}
