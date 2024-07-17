#include "gtest/gtest.h"
#include "system_alloc.h"

int hello() {
    return 0;
}

TEST(hello, hello) {
    EXPECT_EQ(0, hello());
}

TEST(hello, hello1) {
    EXPECT_EQ(0, hello());
}