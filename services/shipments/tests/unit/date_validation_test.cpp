#include <platform/calendar_date.hpp>
#include <platform/date_validation.hpp>
#include <platform/text.hpp>
#include <userver/utest/utest.hpp>
#include <userver/utils/mock_now.hpp>

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

UTEST(CalendarDate, CurrentUtcDateUsesMockedClock) {
    userver::utils::datetime::MockNowSet(std::chrono::sys_days{std::chrono::year{2026} / 11 / 12} +
        std::chrono::hours{23} + std::chrono::minutes{59} + std::chrono::seconds{59});
    EXPECT_EQ(platform::CurrentUtcDate(), (userver::utils::datetime::Date{2026, 11, 12}));
    userver::utils::datetime::MockNowUnset();
}

UTEST(OptionalText, TrimmingPreservesMeaningAndDropsBlankValues) {
    EXPECT_EQ(platform::TrimToOptional(std::nullopt), std::nullopt);
    EXPECT_EQ(platform::TrimToOptional("  "), std::nullopt);
    EXPECT_EQ(platform::TrimToOptional("  груз  "), std::optional<std::string>{"груз"});
}
