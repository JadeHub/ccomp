int main()
{
	struct {int i; struct {int x; int y;} n; int j; } a = {1, {3, 4}, 2};
	return a.i + a.j + a.n.x + a.n.y;
}
