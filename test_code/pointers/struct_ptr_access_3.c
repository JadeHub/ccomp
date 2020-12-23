typedef struct A
{
        int i;
        struct A* next;
}A_t;

int main()
{
        struct A* first;

        A_t a1;
	A_t a2;
        a1.i = 1;
	a1.next = &a2;

        first = &a1;
        return first->i;
}

