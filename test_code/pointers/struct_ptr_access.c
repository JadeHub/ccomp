
int main()
{
	struct A 
	{
		int a; // 0
		struct Sub* s; // -8
		int b;  //-12
	} aVal; 	//-12
	
	struct Sub 
	{ 
		int x; //-16
		int y; //-20
		struct SubSub
		{
			int j;
			int k;
		} ss;
		int z; //-24
	} sVal; // -24

	
	aVal.s = &sVal;

	struct A* aPtr; //-28
	
	aPtr = &aVal;


	aPtr->s->ss.k = 5;
	//aVal.s->y = 6;
	

	return aPtr->s->ss.k;
}
