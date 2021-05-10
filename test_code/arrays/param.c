int fn(int p[], int len)
{
	int total = 0;
	for(int i=0;i<len;i++)
	{
		total+= p[i];
	}
	return total;
}

int main()
{
	int i[5] = {1, 2, 3, 4, 5};
	return fn(i, 5);
}
