#include <gtest/gtest.h>



struct A { int a, b, c; };


struct A foo1()
{
    struct A a;
    a.a = 2;
    return a;
}

struct A foo2()
{
    struct A a;
    a.a = 1;
    return a;
}


void bar()
{
    struct A a, b;
    int i = 0;
    a = i ? foo1() : foo2();

    b = a;
}


int main(int argc, char** argv)
{
    bar();
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
