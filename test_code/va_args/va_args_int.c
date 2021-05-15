#include "va_args.h"

int sum(int count, ...)
{
	va_list args;
        va_start(args, count);
	int sum = 0;
	for(int i=0;i<count;i++)
		sum += va_arg(args, int);
        return sum;
}

int main()
{
	return sum(5, 1, 2, 3, 4, 5);
}
