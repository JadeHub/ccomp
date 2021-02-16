int fn(int a)
{
	return a;
}

int fn2(int a)
{
	return a * 2;
}

typedef int (*fn_t)(int);

int main()
{
	fn_t p[2];

	p[0] = fn;
	p[1] = fn2;

	fn_t f = p[1];
	return f(2);
}
