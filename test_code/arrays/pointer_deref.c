int main()
{
	int* vals[2];
	int p = 5;
	vals[1] = &p;
	*vals[1] = 7;
	return p;
}
