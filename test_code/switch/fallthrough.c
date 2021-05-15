int main()
{
	int c = 2;

	switch (c)
	{
	case 1:
		return 1;
	case 2:
	case 3:
		return 5;
	default:
		return 12;
	}

	return 0;
}
