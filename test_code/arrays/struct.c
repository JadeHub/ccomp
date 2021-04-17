typedef struct
{
	int x; 
	int y;
}s_t;


int main()
{
	s_t v[10];

	for (int i = 0; i < 10; i++)
	{
		v[i].x = 7;
		v[i].y = (i + 1) * 10;
	}

	int total = 0;
	for (int i = 0; i < 10; i++)
		total += v[i].x + v[i].y;
	return total;	
}
