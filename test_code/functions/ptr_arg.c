int fn(int *p)
{
	return *p;
}

int main()
{
	int a = 5;
	return fn(&a);
}
