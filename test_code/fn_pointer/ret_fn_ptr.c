typedef int (*fn_t)(int);

int fn(int i)
{
	return i*2;
}

fn_t fn_ret()
{
	fn_t f = fn;
	return f;
}

int main()
{
	fn_t f = fn_ret();
	return f(3);
}
