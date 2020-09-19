struct Sub {int x; int y; int z;};
struct A {int a; struct Sub* s; int b;};

int main()
{
	struct A aVal;
	aVal.a = 5;
	aVal.b = 7;

	struct Sub sVal;

	sVal.x = 10;
	sVal.y = 20;
	sVal.z = 30;

	aVal.s = &sVal;

	aVal.s->z = 90;

	return aVal.a + aVal.b + aVal.s->x + aVal.s->y + aVal.s->z;
}

  