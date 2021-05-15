typedef struct
{
	int i;
}s_t;

typedef union
{
	int j;
	s_t s;
}u_t;

int main()
{
	u_t u;
	u.j = 5;
	return u.s.i;
}
