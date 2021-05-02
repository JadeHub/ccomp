struct A {int a; char* s; int b;} a = {1, "hello", 2};

int main()
{
	return a.s[3];
}

