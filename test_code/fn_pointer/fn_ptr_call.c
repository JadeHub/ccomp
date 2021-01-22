int fn(int a, char* f)
{
	return a + f[0];
}


int main()
{
	int (*p)(int, char*);
	p = fn;
	return p(1, "Hello");	
}
