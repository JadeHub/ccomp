typedef int (*fn_t)(int);

int fn(int a)
{
	return a + 10;
}

typedef struct
{
	int i;
	fn_t handler;
}item_t;

int handle(item_t* item)
{
	return item->handler(item->i);
}


int main()
{
	item_t item;
	item.i = 8;
	item.handler = fn;
	return handle(&item);
}
