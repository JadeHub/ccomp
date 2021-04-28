int main()
{
	struct A 
	{
		int i : 3;
		int y : 3;
	} a;

	a.i = 1;

	return sizeof(a);
}
