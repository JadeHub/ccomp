#include <gtest/gtest.h>
#include <gmock/gmock.h>

extern "C"
{
#include <libj/include/hash_table.h>
}

TEST(StringHashTable, insert_iter)
{
	hash_table_t* ht = sht_create(3);

	sht_insert(ht, "Test1", (void*)1);
	sht_insert(ht, "Test2", (void*)2);
	sht_insert(ht, "Test3", (void*)3);
	sht_insert(ht, "Test4", (void*)4);
	sht_insert(ht, "Test5", (void*)5);

	sht_insert(ht, "Test10", (void*)1);
	sht_insert(ht, "Test20", (void*)2);
	sht_insert(ht, "Test30", (void*)3);
	sht_insert(ht, "Test40", (void*)4);
	sht_insert(ht, "Test50", (void*)5);

	EXPECT_TRUE(sht_contains(ht, "Test1"));
	EXPECT_TRUE(sht_contains(ht, "Test2"));
	EXPECT_TRUE(sht_contains(ht, "Test3"));
	EXPECT_TRUE(sht_contains(ht, "Test4"));
	EXPECT_TRUE(sht_contains(ht, "Test5"));

	EXPECT_FALSE(sht_contains(ht, "Test6"));

	sht_iterator_t it = sht_begin(ht);

	size_t count = 0;
	while (!sht_end(ht, &it))
	{
		count++;
		sht_next(ht, &it);
	}
	EXPECT_EQ(count, 10);	
	ht_destroy(ht);
}

TEST(StringHashTable, insert_remove)
{
	hash_table_t* ht = sht_create(3);

	EXPECT_TRUE(ht_empty(ht));
	EXPECT_EQ(0, ht_count(ht));
	sht_insert(ht, "Test1", (void*)1);
	EXPECT_FALSE(ht_empty(ht));
	EXPECT_EQ(1, ht_count(ht));
	sht_remove(ht, "Test1");
	EXPECT_TRUE(ht_empty(ht));
	EXPECT_EQ(0, ht_count(ht));
	ht_destroy(ht);
}

TEST(StringHashTable, lots)
{
	char buff[32];

	hash_table_t* ht = sht_create(64);

	for (size_t i = 0; i < 1024; i++)
	{
		sprintf(buff, "test %d", i);
		sht_insert(ht, buff, 0);
	}
	EXPECT_EQ(1024, ht_count(ht));
	ht_print_stats(ht);
	ht_destroy(ht);
}