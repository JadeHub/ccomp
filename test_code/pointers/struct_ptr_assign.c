struct A {int a; int b;};

int main()
{
        struct A i;
        i.a = 5;
        i.b = 7;

        struct A * ptr;
	ptr = &i;

        int j;
	j = (*ptr).a;
        return j;
}

