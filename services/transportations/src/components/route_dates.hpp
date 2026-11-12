#pragma once
#include <chrono>
#include <optional>
#include <string>
#include <userver/utils/datetime/date.hpp>

namespace transportations::route_rules {
enum class DateKind { Planned, Actual };

// Один календарный день и единые границы для всех дат проверяемого участка.
class RouteDateValidation final {
  public:
    RouteDateValidation(
        std::string date_error, std::string sequence_error, std::string sequence_message);
    void ValidateDate(const std::optional<userver::utils::datetime::Date>&, DateKind) const;
    void ValidateOrder(const std::optional<userver::utils::datetime::Date>& from,
        const std::optional<userver::utils::datetime::Date>& to) const;

  private:
    std::chrono::year_month_day today_;
    std::string date_error_;
    std::string sequence_error_;
    std::string sequence_message_;
};
}  // namespace transportations::route_rules
