#include <components/route_dates.hpp>
#include <models/errors.hpp>
#include <platform/date_validation.hpp>
#include <userver/utils/datetime_light.hpp>
#include <utility>

namespace {
constexpr auto kMinimumDate = std::chrono::year{1900} / std::chrono::January / 1;
constexpr auto kMaximumDate = std::chrono::year{9999} / std::chrono::December / 31;
}  // namespace

namespace transportations::route_rules {
RouteDateValidation::RouteDateValidation(
    std::string date_error, std::string sequence_error, std::string sequence_message)
    // Календарь бизнесовых событий — Europe/Moscow.
    : today_(std::chrono::floor<std::chrono::days>(
          userver::utils::datetime::Now() + std::chrono::hours{3})),
      date_error_(std::move(date_error)), sequence_error_(std::move(sequence_error)),
      sequence_message_(std::move(sequence_message)) {}

void RouteDateValidation::ValidateDate(
    const std::optional<userver::utils::datetime::Date>& value, DateKind kind) const {
    if (!value.has_value())
        return;
    const auto maximum = kind == DateKind::Actual ? today_ : kMaximumDate;
    if (!platform::IsCalendarDateInRange(value.value(), kMinimumDate, maximum))
        throw ApiError<400>{date_error_,
            kind == DateKind::Actual
                ? "Фактическая дата должна быть от 01.01.1900 до сегодняшнего дня"
                : "Укажите корректную календарную дату от 01.01.1900"};
}

void RouteDateValidation::ValidateOrder(const std::optional<userver::utils::datetime::Date>& from,
    const std::optional<userver::utils::datetime::Date>& to) const {
    if (from.has_value() && to.has_value() && from.value() > to.value())
        throw ApiError<400>{sequence_error_, sequence_message_};
}
}  // namespace transportations::route_rules
