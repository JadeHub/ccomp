int main()
{
	struct {int i; struct {char x, y;} n; int j; } a = {1, {3, 4}, 2};
	return a.i + a.j + a.n.x + a.n.y;
}
