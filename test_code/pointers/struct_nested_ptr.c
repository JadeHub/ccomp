struct Sub {int x; int y; int z;};
struct A {int a; struct Sub* s; int b;};

int main()
{
	struct A aVal;

	struct Sub sVal;

	aVal.s = &sVal;

	aVal.s->z = 90;

	return aVal.s->z;
}

  
