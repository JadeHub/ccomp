typedef struct
{
	int i;
	int j;
}s_t;


int main()
{
	s_t v[10];
	
	v[4].i = 7;
	v[4].j = 10;
	return v[4].i * v[4].j;
}
