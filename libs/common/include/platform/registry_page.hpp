#pragma once
#include <charconv>
#include <limits>
#include <platform/handler_error.hpp>
#include <platform/registry_filters.hpp>

namespace platform::registry {
// Числа передаются в JSON-курсоре строкой, но проверяются по C++-типу ключа.
inline void ValidateCursorKey(CursorKeyType type, std::string_view value) {
    if (type != CursorKeyType::Int32 && type != CursorKeyType::Int64)
        return;
    std::int64_t number{};
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), number);
    if (error != std::errc{} || end != value.data() + value.size() ||
        (type == CursorKeyType::Int32 &&
            (number < std::numeric_limits<std::int32_t>::min() ||
                number > std::numeric_limits<std::int32_t>::max())))
        throw platform::http::HandlerError<400>{
            "invalid_cursor", "Некорректное числовое значение курсора"};
}

template <typename Cursor>
void ValidateListCursor(
    const std::optional<Cursor>& cursor, const std::string& scope, bool date_sort = false) {
    if (!cursor)
        return;
    bool valid = cursor->scope == scope;
    if constexpr (requires {
                      cursor->date_value;
                      cursor->text_value;
                  })
        valid = valid && (date_sort == cursor->date_value.has_value()) &&
            (date_sort != cursor->text_value.has_value());
    if (!valid)
        throw platform::http::HandlerError<400>{
            "invalid_cursor", "Курсор не соответствует фильтрам или сортировке"};
}

template <typename Cursor, typename Id, typename Row>
Cursor MakeListCursor(
    const Id& id, const Row& row, const std::string& scope, bool date_sort = false) {
    if constexpr (requires {
                      row.DateKey;
                      row.TextKey;
                  })
        return {.date_value = date_sort ? row.DateKey : std::nullopt,
            .id = id,
            .scope = scope,
            .text_value = date_sort ? std::nullopt : std::optional{row.TextKey}};
    else
        return {.id = id, .scope = scope, .value = row.SortKey};
}

// Запрос читает на одну строку больше. Курсор указывает на последнюю возвращаемую строку.
template <typename Cursor, typename Row, typename GetId>
std::optional<Cursor> FinishListPage(std::vector<Row>& rows, std::size_t limit,
    const std::string& scope, GetId get_id, bool date_sort = false) {
    if (rows.size() <= limit || limit == 0)
        return std::nullopt;
    rows.resize(limit);
    const auto& last = rows.back();
    return MakeListCursor<Cursor>(get_id(last), last, scope, date_sort);
}

// Каталог и ограничения области остаются в сервисе. Курсор и границы страницы — общие.
template <typename Request>
BuiltQuery BuildPage(
    QueryBuilder builder, std::span<const RegistryField> fields, const Request& request) {
    using Cursor = typename std::remove_cvref_t<decltype(request.cursor)>::value_type;
    std::optional<decltype(Cursor::id)> after;
    if (request.cursor.has_value())
        after = request.cursor.value().id;
    else if constexpr (requires { request.after; })
        after = request.after;
    if constexpr (requires(Cursor cursor) {
                      cursor.date_value;
                      cursor.text_value;
                  }) {
        const auto order = RegistryOrderBy(fields, request.sort);
        if (after.has_value() && order.CursorType == CursorKeyType::Date)
            builder.After(after, request.cursor.value().date_value.value());
        else {
            const auto key =
                request.cursor.has_value() ? request.cursor.value().text_value.value_or("") : "";
            if (after.has_value())
                ValidateCursorKey(order.CursorType, key);
            builder.After(after, key);
        }
    } else {
        const auto key = request.cursor.has_value() ? request.cursor.value().value : "";
        if (after.has_value())
            ValidateCursorKey(RegistryOrderBy(fields, request.sort).CursorType, key);
        builder.After(after, key);
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
