#include <gtest/gtest.h>




int main(int argc, char** argv) 
{
	

	size_t lli = sizeof( short );
	size_t ill = sizeof( long long);
	size_t lil = sizeof(long   long);
	size_t i = sizeof(int);
	size_t l = sizeof(long);
	size_t u = sizeof(unsigned);
	size_t s = sizeof(signed);

	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
