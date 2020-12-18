enum E {E1, E2};
struct S {enum E e;};


int main()
{
	struct S s;
	s.e = E1;
	return s.e;
}
