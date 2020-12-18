enum colour
{
	red, blue, green
};

struct Car
{
	int wheels; 
	enum colour colour;
};

struct Car MakeCar(int wheels, enum colour colour)
{
	struct Car car;

	car.wheels = wheels;
	car.colour = colour;
	return car;
}

int add(int i, int j)
{
	return i + j;
}

int mul(int i, int j)
{
	return i * j;
}

int main()
{
	enum colour c = red;

	struct Car carA = MakeCar(2, blue);

	struct Car carB = MakeCar(7, green);

	struct Car carC = carA;

	struct Boat
	{
		int hp;
	};

	int j = !carC.colour;

	j = add(j, 7);

	for (int i = 0; i < 10; i++)
	{
		j = j + 1;
	}

	return mul(j, 7);
}
