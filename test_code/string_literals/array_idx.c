char* m = "Hello";

char* fn()
{
	return m;
}

int idx()
{
	return 2;
}

int main()
{
	return fn()[idx()];
}
