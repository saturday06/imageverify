#include <gtest/gtest.h>
#include "imageverify.h"

int add(int x, int y)
{
    return x + y;
}

TEST(AddTest, Test1)
{
	for (int i = 0; i < 100; ++i) {
	    ASSERT_EQ(1 + i, add(1, i));
	    ASSERT_EQ(0, image_verify());
	}
}
