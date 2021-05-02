struct A {int a; struct {int x, y;} n; int b;} a = {1, {3, 4}, 2};

int main()
{
	return a.a + a.b + a.n.x + a.n.y;
}

