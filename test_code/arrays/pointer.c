int main()
{
	int* vals[2];
	int p = 5;
	vals[1] = &p;
	return *vals[1];
}
