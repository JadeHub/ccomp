int main()
{
	int i = 5;
	int* ptr = &i;
	int** pp = &ptr;

	**pp = 11;

	return **pp;
}
