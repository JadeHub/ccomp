int main()
{
	char i = 5;
	char j = 7;
	char* ptr = &i;

	*ptr = j;

	return *ptr;
}
