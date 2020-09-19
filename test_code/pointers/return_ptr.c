int g = 5;

int* fn()
{
	return &g;
}

int main()
{
	return *fn();
}
