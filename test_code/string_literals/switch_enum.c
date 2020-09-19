
int main()
{
	int c = 1;

	switch (c)
	{
	case 1:
		break;
	case 2:
	case 3:
		return 5;
	default:
		return 12;
	}

	return 0;
}
