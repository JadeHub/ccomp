typedef struct A
{
        int i;
        struct A* next;
}A_t;

int main()
{
        A_t* first;

        A_t a1;
	A_t a2;
	A_t a3;
	A_t a4;

        a1.i = 1;
        a2.i = 2;
        a3.i = 3;
        a4.i = 4;

        a1.next = &a2;
        a2.next = &a3;
        a3.next = &a4;
        a4.next = 0;

        int total = 0;
        first = &a1;
        while(first)
        {
                total = total + first->i;
                first = first->next;
        }

        return total;
}

