int main()
{
	struct { int i; int j; } s;
	struct { int x; int y; } p;

	s.i = 5;
	s.j = -10;
	p.x = 1;
	p.y = 2;
	return s.i + s.j + p.x + p.y;
}
