int main()
{
	int i[5][2];

	int (*p)[5][2] = &i;
	(*p)[0][1] = 5;
	
	return i[0][1];
}
