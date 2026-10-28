#pragma once
#include <boost/uuid/string_generator.hpp>
#include <platform/registry_query.hpp>
#include <set>
#include <userver/utils/datetime/date.hpp>

namespace platform::registry {
class FilterError : public std::invalid_argument {
  public:
    using std::invalid_argument::invalid_argument;
};

struct RegistryField {
    FieldDefinition Field;
    std::string_view Label;
    std::string_view ValueSql;
    bool Sortable = true;
    bool FoldValues = false;
    bool Array = false;
    bool Visible = true;
    std::string_view Lookup{};

    std::string_view Kind() const {
        return Array                                  ? "set"
            : Field.CursorType == CursorKeyType::Date ? "date"
            : Field.PgType == "BOOLEAN"               ? "boolean"
                                                      : "values";
    }

    std::string_view MatchSql() const {
        return FoldValues && !Field.SortExpression.empty() ? Field.SortExpression
                                                           : Field.Expression;
    }
};

template <std::size_t N>
constexpr std::array<FieldDefinition, N> SortableFields(
    const std::array<RegistryField, N>& fields) {
    std::array<FieldDefinition, N> result{};
    for (std::size_t index = 0; index < N; ++index)
        if (fields[index].Sortable)
            result[index] = fields[index].Field;
    return result;
}

inline const RegistryField& FindField(
    std::span<const RegistryField> fields, std::string_view name) {
    for (const auto& field : fields)
        if (field.Field.Name == name)
            return field;
    throw FilterError{"Поле не поддерживает фильтрацию"};
}

inline Order RegistryOrderBy(std::span<const RegistryField> fields, std::string_view name) {
    const auto& field = FindField(fields, name);
    if (!field.Sortable)
        throw FilterError{"Сортировка по этому полю недоступна"};
    const std::array<FieldDefinition, 1> definition{field.Field};
    return OrderBy(definition, name);
}

template <typename Filters>
bool UseLegacyFilter(
    const Filters& filters, std::string_view field, const std::optional<std::string>& exclude) {
    if (exclude.has_value() && exclude.value() == field)
        return false;
    if (filters.has_value())
        for (const auto& filter : filters.value())
            if (filter.field == field)
                return false;
    return true;
}

inline void ValidateValue(const RegistryField& field, const std::string& value) {
    if (value.empty())
        return;
    if (field.Field.PgType == "BOOLEAN" && value != "true" && value != "false")
        throw FilterError{"Некорректное логическое значение"};
    if (field.Field.PgType == "UUID")
        try {
            boost::uuids::string_generator{}(value);
        } catch (const std::exception&) {
            throw FilterError{"Некорректный идентификатор"};
        }
}

inline void ApplyCandidates(QueryBuilder& builder, const RegistryField& field,
    const std::optional<std::vector<std::string>>& candidates) {
    if (!candidates.has_value())
        return;
    for (const auto& value : candidates.value())
        ValidateValue(field, value);
    auto selected = field.Field;
    selected.Expression = field.MatchSql();
    builder.WhereValues(selected, candidates.value(), field.FoldValues, field.Array);
}

template <typename Filters>
void ApplyFilters(QueryBuilder& builder, std::span<const RegistryField> fields,
    const Filters& filters, const std::optional<std::string>& exclude = std::nullopt) {
    if (!filters.has_value())
        return;
    std::set<std::string> used;
    for (const auto& filter : filters.value()) {
        const auto& field = FindField(fields, filter.field);
        if (!used.insert(filter.field).second)
            throw FilterError{"Условия одного поля должны быть объединены"};
        const bool range = filter.date_from.has_value() || filter.date_to.has_value();
        const int modes =
            filter.values.has_value() + filter.query.has_value() + filter.empty.has_value() + range;
        if (modes != 1)
            throw FilterError{"Укажите один способ фильтрации поля"};
        if (range && field.Kind() != "date")
            throw FilterError{"Диапазон дат доступен только для даты"};
        if (field.Kind() == "date" && !range && !filter.empty.has_value())
            throw FilterError{"Для даты укажите диапазон или заполненность"};
        if (filter.date_from.has_value() && filter.date_to.has_value() &&
            filter.date_from.value() > filter.date_to.value())
            throw FilterError{"Начало периода позже окончания"};
        if (filter.query.has_value() && (field.Array || field.Kind() != "values"))
            throw FilterError{"Поиск по вхождению недоступен для этого поля"};
        if (filter.values.has_value())
            for (const auto& value : filter.values.value())
                ValidateValue(field, value);
        if (exclude.has_value() && filter.field == exclude.value())
            continue;
        auto definition = field.Field;
        definition.Expression = field.MatchSql();
        if (filter.values.has_value())
            builder.WhereValues(definition, filter.values.value(), field.FoldValues, field.Array);
        if (filter.query.has_value()) {
            const auto expression = field.FoldValues ? std::string{field.Field.Expression}
                                                     : "lower(COALESCE((" +
                    std::string{field.Field.Expression} + ")::TEXT, '')) COLLATE \"C\"";
            builder.WhereContains(Field<std::string>{field.Field.Name, expression}, filter.query);
        }
        if (filter.empty.has_value())
            builder.WhereEmpty(definition, filter.empty.value(), field.Array);
        const Field<userver::utils::datetime::Date> date{
            field.Field.Name, field.Field.Expression, "DATE"};
        builder.WhereAtLeast(date, filter.date_from);
        builder.WhereAtMost(date, filter.date_to);
    }
}

inline BuiltQuery BuildValues(QueryBuilder builder, const pg::Query& selection,
    const RegistryField& field, const std::optional<std::string>& search,
    const std::optional<std::string>& after, int limit) {
    if (field.Kind() == "date")
        throw FilterError{"Для даты используйте диапазон"};
    if (after.has_value())
        ValidateValue(field, after.value());
    std::string key =
        field.Array ? "registry_value.value COLLATE \"C\"" : std::string{field.MatchSql()};
    const std::string label = field.Array ? "registry_value.value" : std::string{field.ValueSql};
    const std::string join = field.Array
        ? "CROSS JOIN LATERAL unnest(" + std::string{field.ValueSql} + ") AS registry_value(value)"
        : "";
    // NULL и пустая строка имеют одно отображение «Не заполнено».

    const auto search_expression = field.FoldValues
        ? std::string{field.Field.Expression}
        : "lower(COALESCE((" + label + ")::TEXT, '')) COLLATE \"C\"";
    builder.WhereContains(Field<std::string>{"facet_query", search_expression}, search);
    return std::move(builder).BuildValues(
        selection, key, label, join, after, limit, field.Array ? "TEXT" : field.Field.PgType);
}

struct ValueRow {
    std::string Value;
    std::string Label;
};

template <typename Model> std::vector<Model> DescribeFields(std::span<const RegistryField> fields) {
    std::vector<Model> result;
    for (const auto& field : fields) {
        if (!field.Visible)
            continue;
        Model value;
        value.id = field.Field.Name;
        value.label = field.Label;
        value.kind = ParseEnumScalar(std::string{field.Kind()}, decltype(value.kind){});
        value.sortable = field.Sortable;
        value.case_insensitive = field.FoldValues;
        if (!field.Lookup.empty())
            value.lookup = field.Lookup;
        result.push_back(std::move(value));
    }
    return result;
}
}  // namespace platform::registry
