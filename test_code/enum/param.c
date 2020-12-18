enum E {E1, E2};

int fn(enum E e)
{
	return e;
}

int main()
{
	enum E e = E1;
	return fn(e);
}
