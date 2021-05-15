int main()
{
	union
	{
		int i;
		short s;
		char c;
	} u;

	u.i = 0x12345678;
	return u.c;
}
