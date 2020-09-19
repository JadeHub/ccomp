int fn(int* p, char* c)
{
	return *p * *c;
}

int main()
{
	char c = 7;
	int i = 5;
	return fn(&i, &c);
}
