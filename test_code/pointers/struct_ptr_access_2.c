
int main()
{

	struct Sub
	{
		int x; //-16
		int y; //-20
		int z; //-24
	} ; // -24

	struct A 
	{
		int a; // 0
		struct Sub s; // -12
		int b;  //-16
	} aVal; 	//-16
	
	struct A* aPtr; //-20
	
	aPtr = &aVal;


	aPtr->s.y = 7;


	return aPtr->s.y;
}
