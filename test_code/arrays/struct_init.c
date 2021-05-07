int main()
{
	struct A
	{
		int v[2];
	};

	struct A a[2] = { {{1, 2}}, {{3, 4}} };

	return a[0].v[0] + a[0].v[1] + a[1].v[0] + a[1].v[1];
}
