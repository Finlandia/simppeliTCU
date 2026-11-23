// Basic unit tests for simppeliTCU/stringBuffer.h
#include <gtest/gtest.h>

#include "stringBuffer.h"

TEST(StringBuffer, StartsEmpty) {
    StringBuffer<8> s;
    EXPECT_TRUE(s.isEmpty());
    EXPECT_EQ(s.length(), 0u);
    EXPECT_EQ(s.capacity(), 8u);
}

TEST(StringBuffer, CopyFromTruncatesToCapacity) {
    StringBuffer<5> s;
    s.copyFrom("abcdefgh");
    EXPECT_STREQ(s.c_str(), "abcd");
}

TEST(StringBuffer, CopyFromDataTerminatesString) {
    StringBuffer<16> s;
    const byte data[] = {'h', 'i', '!', 'x'};
    s.copyFromData(data, 3);
    EXPECT_STREQ(s.c_str(), "hi!");
}

TEST(StringBuffer, FormatAndAppendTruncate) {
    StringBuffer<8> s;
    s.format("%d-%s", 42, "ab");
    EXPECT_STREQ(s.c_str(), "42-ab");
    s.append("cdef");
    EXPECT_STREQ(s.c_str(), "42-abcd");
    s.clear();
    EXPECT_TRUE(s.isEmpty());
}

TEST(StringBuffer, CompareHelpers) {
    StringBuffer<16> s;
    s.copyFrom("lock 1234");
    EXPECT_TRUE(s.startsWith("lock"));
    EXPECT_FALSE(s.startsWith("unlock"));
    EXPECT_TRUE(s.equals("lock 1234"));
    EXPECT_FALSE(s.equals("lock"));
}

TEST(StringBuffer, TrimAndLowerCase) {
    StringBuffer<32> s;
    s.copyFrom("  Charge START \r\n");
    s.trim();
    s.toLowerCase();
    EXPECT_STREQ(s.c_str(), "charge start");

    s.copyFrom("   ");
    s.trim();
    EXPECT_TRUE(s.isEmpty());
}
