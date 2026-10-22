#include <platform/date_validation.hpp>
#include <userver/utest/utest.hpp>

UTEST(CalendarDate, RejectsInvalidDates) {
    for (const auto value :
        {"2025-02-29", "2026-04-31", "2026-13-01", "0000-01-01", "2026-1-1", "2026-01-01x"})
        EXPECT_FALSE(platform::ParseCalendarDate(value).has_value()) << value;
}

UTEST(CalendarDate, CalendarAndCallerRangeAreSeparate) {
    using namespace std::chrono;
    EXPECT_TRUE(platform::ParseCalendarDate("1111-11-11").has_value());
    EXPECT_TRUE(platform::ParseCalendarDate("2000-02-29").has_value());
    EXPECT_FALSE(platform::IsCalendarDateInRange(
        "1111-11-11", year{1900} / January / 1, year{9999} / December / 31));
    EXPECT_TRUE(platform::IsCalendarDateInRange(
        "2099-12-31", year{1900} / January / 1, year{9999} / December / 31));
}
