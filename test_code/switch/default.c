int main()
{
	int c = 10;

	switch (c)
	{
	case 1:
		return c;
	case 2:
	case 3:
		return 5;
	default:
		return 12;
	}

	return 0;
}
