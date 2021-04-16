int main()
{
	int test1[5];
	test1[3] = 7;
	int* test = test1;
//	test[3] = 6;
	return test[3];
}
