#pragma once
#include <platform/registry_filters.hpp>

namespace platform::registry {
// Каталог и ограничения области остаются в сервисе. Курсор и границы страницы — общие.
template <typename Request>
BuiltQuery BuildPage(
    QueryBuilder builder, std::span<const RegistryField> fields, const Request& request) {
    std::optional<std::string> after;
    if (request.cursor.has_value())
        after = request.cursor.value().id;
    else if constexpr (requires { request.after; })
        after = request.after;
    using Cursor = typename std::remove_cvref_t<decltype(request.cursor)>::value_type;
    if constexpr (requires(Cursor cursor) {
                      cursor.date_value;
                      cursor.text_value;
                  }) {
        const auto order = RegistryOrderBy(fields, request.sort);
        if (after.has_value() && order.CursorType == CursorKeyType::Date)
            builder.After(after, request.cursor.value().date_value.value());
        else
            builder.After(after,
                request.cursor.has_value() ? request.cursor.value().text_value.value_or("") : "");
    } else {
        builder.After(after, request.cursor.has_value() ? request.cursor.value().value : "");
    }
    return std::move(builder).Build(request.limit + 1);
}

inline BuiltQuery BuildFilterValues(QueryBuilder builder, std::span<const RegistryField> fields,
    const pg::Query& selection, const std::string& field, const std::optional<std::string>& search,
    const std::optional<std::string>& after, int limit,
    const std::optional<std::vector<std::string>>& candidates = std::nullopt) {
    const auto& definition = FindField(fields, field);
    ApplyCandidates(builder, definition, candidates);
    return BuildValues(std::move(builder), selection, definition, search, after, limit + 1);
}

// Описание реестра связывает каталог и SQL с ограничениями конкретного сервиса.
template <typename Request, typename Context> struct Definition {
    std::span<const RegistryField> Fields;
    const pg::Query& Values;
    QueryBuilder (*Filters)(const Request&, const Context&, const std::optional<std::string>&);
};

template <typename Request, typename Context>
BuiltQuery BuildPage(const Definition<Request, Context>& definition, const Request& request,
    const Context& context) {
    return BuildPage(
        definition.Filters(request, context, std::nullopt), definition.Fields, request);
}

template <typename Request, typename Context>
BuiltQuery BuildFilterValues(const Definition<Request, Context>& definition, const Request& request,
    const Context& context, const std::string& field, const std::optional<std::string>& search,
    const std::optional<std::string>& after, int limit,
    const std::optional<std::vector<std::string>>& candidates = std::nullopt) {
    return BuildFilterValues(definition.Filters(request, context, field), definition.Fields,
        definition.Values, field, search, after, limit, candidates);
}

template <typename Request, typename Context, typename ValuesRequest>
BuiltQuery BuildFilterValues(const Definition<Request, Context>& definition,
    const ValuesRequest& request, const Context& context) {
    return BuildFilterValues(definition, request.registry, context, request.field.value(),
        request.query, request.after, request.limit, request.candidates);
}
}  // namespace platform::registry
