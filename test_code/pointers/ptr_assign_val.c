int main()
{
	int i = 5;
	int j = 7;
	int* ptr = &i;

	*ptr = j;

	return *ptr;
}
