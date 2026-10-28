#pragma once
#include <optional>
#include <vector>

namespace web_bff {
template <typename Out, typename In>
std::optional<std::vector<Out>> ConvertRegistryFilters(
    const std::optional<std::vector<In>>& filters) {
    if (!filters.has_value())
        return std::nullopt;
    std::vector<Out> result;
    for (const auto& filter : filters.value())
        result.push_back({.date_from = filter.date_from,
            .date_to = filter.date_to,
            .empty = filter.empty,
            .field = filter.field,
            .query = filter.query,
            .values = filter.values});
    return result;
}

template <typename Out, typename In> Out ConvertRegistryField(const In& field) {
    Out result;
    result.id = field.id;
    result.case_insensitive = field.case_insensitive;
    result.label = field.label;
    result.lookup = field.lookup;
    result.sortable = field.sortable;
    result.kind = ParseEnumScalar(EnumValue(field.kind), decltype(result.kind){});
    return result;
}
}  // namespace web_bff
