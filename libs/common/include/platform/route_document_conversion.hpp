#pragma once
#include <optional>
#include <vector>

namespace platform {
namespace detail {
template <typename Source, typename Target>
void CopyRouteEnum(const std::optional<Source>& value, std::optional<Target>& result) {
    if (value.has_value())
        result = ParseEnumScalar(EnumValue(value.value()), Target{});
}
}  // namespace detail

template <typename Target, typename Source>
std::optional<Target> CopyRouteBooking(const std::optional<Source>& booking) {
    if (!booking.has_value())
        return std::nullopt;
    const auto& value = booking.value();
    Target result{.confirmed_date = value.confirmed_date,
        .number = value.number,
        .placed_date = value.placed_date};
    detail::CopyRouteEnum(value.status, result.status);
    return result;
}

template <typename Target, typename Source>
std::optional<std::vector<Target>> CopyBillsOfLading(
    const std::optional<std::vector<Source>>& documents) {
    if (!documents.has_value())
        return std::nullopt;
    std::vector<Target> result;
    result.reserve(documents.value().size());
    for (const auto& document : documents.value()) {
        Target row{.date = document.date, .number = document.number};
        detail::CopyRouteEnum(document.leg, row.leg);
        result.push_back(row);
    }
    return result;
}

template <typename Target, typename Source>
std::optional<std::vector<Target>> CopyAirWaybills(
    const std::optional<std::vector<Source>>& documents) {
    if (!documents.has_value())
        return std::nullopt;
    std::vector<Target> result;
    result.reserve(documents.value().size());
    for (const auto& document : documents.value())
        result.push_back({.date = document.date, .number = document.number});
    return result;
}

template <typename Target, typename Source>
std::optional<Target> CopyRouteDocumentEvent(const std::optional<Source>& event) {
    if (!event.has_value())
        return std::nullopt;
    return Target{.date = event.value().date};
}

}  // namespace platform
