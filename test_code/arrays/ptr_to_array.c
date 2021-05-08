int main()
{
	int i[2] = {1, 2};
	int (*p)[2] = &i;

	return (*p)[1];
}
