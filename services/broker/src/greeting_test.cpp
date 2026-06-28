#include <greeting.hpp>

#include <userver/utest/utest.hpp>

using broker::UserType;

UTEST(SayHelloTo, Basic) {
    EXPECT_EQ(broker::SayHelloTo("Developer", UserType::kFirstTime), "Hello, Developer!\n");
    EXPECT_EQ(broker::SayHelloTo({}, UserType::kFirstTime), "Hello, unknown user!\n");

    EXPECT_EQ(broker::SayHelloTo("Developer", UserType::kKnown), "Hi again, Developer!\n");
    EXPECT_EQ(broker::SayHelloTo({}, UserType::kKnown), "Hi again, unknown user!\n");
}