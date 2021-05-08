int main()
{
	int i[5][2];

	int (*p)[2] = &i[0];
	(*p)[1] = 5;
	
	return i[0][1];
}
