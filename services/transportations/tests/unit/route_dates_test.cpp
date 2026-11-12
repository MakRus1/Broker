#include <components/route_dates.hpp>
#include <models/errors.hpp>
#include <userver/utest/utest.hpp>
#include <userver/utils/mock_now.hpp>

UTEST(RouteDates, ActualDatesUseMoscowCalendarAndPlannedDatesAllowFuture) {
    using namespace std::chrono;
    using userver::utils::datetime::Date;
    userver::utils::datetime::MockNowSet(sys_days{year{2026} / November / 12} + hours{21});
    const transportations::route_rules::RouteDateValidation dates{
        "invalid_road_date", "invalid_road_sequence", "Проверьте последовательность"};
    userver::utils::datetime::MockNowUnset();

    using transportations::route_rules::DateKind;
    EXPECT_NO_THROW(dates.ValidateDate(Date{2026, 11, 13}, DateKind::Actual));
    EXPECT_THROW(
        dates.ValidateDate(Date{2026, 11, 14}, DateKind::Actual), transportations::ApiError<400>);
    EXPECT_NO_THROW(dates.ValidateDate(Date{9999, 12, 31}, DateKind::Planned));
    EXPECT_THROW(
        dates.ValidateDate(Date{1899, 12, 31}, DateKind::Planned), transportations::ApiError<400>);
    EXPECT_NO_THROW(dates.ValidateDate(std::nullopt, DateKind::Actual));
}

UTEST(RouteDates, SequenceAllowsMissingOrEqualDatesAndRejectsReversedDates) {
    using userver::utils::datetime::Date;
    const transportations::route_rules::RouteDateValidation dates{
        "invalid_rail_date", "invalid_rail_sequence", "Проверьте последовательность"};
    EXPECT_NO_THROW(dates.ValidateOrder(std::nullopt, Date{2026, 11, 12}));
    EXPECT_NO_THROW(dates.ValidateOrder(Date{2026, 11, 12}, Date{2026, 11, 12}));
    EXPECT_THROW(dates.ValidateOrder(Date{2026, 11, 13}, Date{2026, 11, 12}),
        transportations::ApiError<400>);
}
