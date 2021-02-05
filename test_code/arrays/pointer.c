int main()
{
	int* vals[2];
	int p = 5;
	vals[0] = &p;
	return *vals[0];
}
