#include "gtest/gtest.h"
extern int hello();
TEST(hello, hello2) {
    EXPECT_EQ(0, hello());
}

TEST(hello, hello3) {
    EXPECT_EQ(0, hello());
}