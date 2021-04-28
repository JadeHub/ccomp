int main()
{
	struct
	{
		struct {char c; short s;} m[10];
		
	} s;
	return sizeof(s);
}
