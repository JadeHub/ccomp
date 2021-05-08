int main(void)
{
	int buff[10];
	buff[5] = 5;
	int* p = buff;
	p[5] = 10;
	return p[5];
}
