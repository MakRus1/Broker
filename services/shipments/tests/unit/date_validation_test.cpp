#include <platform/date_validation.hpp>
#include <userver/utest/utest.hpp>

UTEST(CalendarDate, CalendarAndCallerRangeAreSeparate) {
    using namespace std::chrono;
    const auto early = userver::utils::datetime::DateFromRFC3339String("1111-11-11");
    const auto future = userver::utils::datetime::DateFromRFC3339String("2099-12-31");
    EXPECT_FALSE(platform::IsCalendarDateInRange(
        early, year{1900} / January / 1, year{9999} / December / 31));
    EXPECT_TRUE(platform::IsCalendarDateInRange(
        future, year{1900} / January / 1, year{9999} / December / 31));
}

UTEST(CalendarDate, NativeDateChecksBusinessRangeWithoutParsing) {
    using namespace std::chrono;
    using userver::utils::datetime::Date;
    EXPECT_TRUE(platform::IsCalendarDateInRange(
        Date{2000, 2, 29}, year{1900} / January / 1, year{9999} / December / 31));
    EXPECT_FALSE(platform::IsCalendarDateInRange(
        Date{1899, 12, 31}, year{1900} / January / 1, year{9999} / December / 31));
}
