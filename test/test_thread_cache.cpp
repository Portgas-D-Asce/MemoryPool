#include "gtest/gtest.h"

int hello() {
    return 0;
}

TEST(hello, hello2) {
    EXPECT_EQ(0, hello());
}

TEST(hello, hello3) {
    EXPECT_EQ(0, hello());
}