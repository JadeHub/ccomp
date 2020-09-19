int main()
{
	int i = 5;
	int* ptr = &i;
	int** pp = &ptr;
	int*** ppp = &pp;

	***ppp = 11;

	return i;
}
