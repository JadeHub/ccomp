int main()
{
	union
	{
		int i; short s; char c;
	} u;
	
	return sizeof(u);
}
