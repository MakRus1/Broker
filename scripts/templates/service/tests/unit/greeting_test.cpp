#include <domain/greeting.hpp>

#include <userver/utest/utest.hpp>

using broker::domain::UserType;

UTEST(SayHelloTo, Basic) {
    EXPECT_EQ(broker::domain::SayHelloTo("Developer", UserType::kFirstTime), "Hello, Developer!\n");
    EXPECT_EQ(broker::domain::SayHelloTo({}, UserType::kFirstTime), "Hello, unknown user!\n");

    EXPECT_EQ(broker::domain::SayHelloTo("Developer", UserType::kKnown), "Hi again, Developer!\n");
    EXPECT_EQ(broker::domain::SayHelloTo({}, UserType::kKnown), "Hi again, unknown user!\n");
}