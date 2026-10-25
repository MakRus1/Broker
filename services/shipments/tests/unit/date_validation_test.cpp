#include <platform/calendar_date.hpp>
#include <platform/date_validation.hpp>
#include <userver/utest/utest.hpp>

UTEST(CalendarDate, CalendarAndCallerRangeAreSeparate) {
    using namespace std::chrono;
    const auto early = platform::ParseCalendarDate("1111-11-11");
    const auto future = platform::ParseCalendarDate("2099-12-31");
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

UTEST(CalendarDate, FullCalendarRangeNeverUsesNanosecondTimePoints) {
    for (const auto& text :
        {"0001-01-01", "1111-11-11", "1900-01-01", "2263-01-01", "9999-12-31"}) {
        EXPECT_EQ(platform::FormatCalendarDate(platform::ParseCalendarDate(text)), text);
    }
}
