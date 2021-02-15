
int main()
{

	struct Sub
	{
		int x; // -8
		int y; // -12
		int z; // -16
	};

	struct A 
	{
		int a; // -4
		struct Sub s; // -16
		int b;  // -20
	} aVal; 	// -20
	
	struct A* aPtr; // -24
	
	aPtr = &aVal;

	aPtr->s.y = 7;

	return aVal.s.y;
}
