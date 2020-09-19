
int main()
{
	struct B
	{
		int x; //0
		int y; //4
		int z; //8
	};

	struct A 
	{
		int a;		//0
		struct B b;	//4
		int c;		//16
	} var;

//	a.b.x = 1;
//	a.b.y = 20;
	var.b.z = 400;
	var.c = 40000;

//	return a.a + a.b.x + a.b.y + a.b.z + a.c;

	return var.b.z + var.c;
}

