#pragma once

#include <array>
#include <fmt/format.h>
#include <optional>
#include <platform/registry.hpp>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <userver/storages/postgres/parameter_store.hpp>
#include <userver/storages/postgres/query.hpp>
#include <userver/utils/datetime/date.hpp>
#include <vector>

namespace platform::registry {
namespace pg = userver::storages::postgres;

enum class CursorKeyType { Text, Date };

template <typename T>
inline constexpr CursorKeyType kCursorKeyType =
    std::is_same_v<T, userver::utils::datetime::Date> ? CursorKeyType::Date : CursorKeyType::Text;

// SQL-выражения задаёт разработчик. Значения запроса всегда идут через ParameterStore.
struct FieldDefinition {
    std::string_view Name;
    std::string_view Expression;
    std::string_view PgType = "TEXT";
    bool ById = false;
    std::string_view SortExpression;
    std::string_view SortPgType;
    CursorKeyType CursorType = CursorKeyType::Text;
    CursorKeyType SortCursorType = CursorKeyType::Text;
};

template <typename T> struct Field : FieldDefinition {
    constexpr Field(std::string_view name, std::string_view expression,
        std::string_view pg_type = "TEXT", bool by_id = false)
        : FieldDefinition{
              name, expression, pg_type, by_id, {}, {}, kCursorKeyType<T>, CursorKeyType::Text} {}

    template <typename SortKey = std::string>
    constexpr Field SortedAs(std::string_view expression, std::string_view pg_type = "TEXT") const {
        auto field = *this;
        field.SortExpression = expression;
        field.SortPgType = pg_type;
        field.SortCursorType = kCursorKeyType<SortKey>;
        return field;
    }
};

struct Order {
    std::string_view Expression;
    bool ById = false;
    std::string_view PgType = "TEXT";
    CursorKeyType CursorType = CursorKeyType::Text;
};

inline Order OrderBy(std::span<const FieldDefinition> fields, std::string_view name) {
    for (const auto& field : fields) {
        if (field.Name == name)
            return {field.SortExpression.empty() ? field.Expression : field.SortExpression,
                field.ById, field.SortPgType.empty() ? field.PgType : field.SortPgType,
                field.SortExpression.empty() ? field.CursorType : field.SortCursorType};
    }
    throw std::logic_error{"Поле не зарегистрировано для сортировки"};
}

template <typename Item> struct Row {
    Item Value;
    std::string SortKey;
};

template <typename Item, typename Converter>
auto ConvertItems(const std::vector<Row<Item>>& rows, Converter convert) {
    std::vector<decltype(convert(std::declval<const Item&>()))> result;
    result.reserve(rows.size());
    for (const auto& row : rows)
        result.push_back(convert(row.Value));
    return result;
}

template <typename Request>
bool CursorMatches(const Request& request, const std::string& scope, bool identity_order) {
    if (request.after.has_value() &&
        (request.cursor.has_value() || !identity_order || request.descending))
        return false;
    return !request.cursor.has_value() || request.cursor.value().scope == scope;
}

struct BuiltQuery {
    pg::Query Statement;
    pg::ParameterStore Parameters;
};

// Ограниченный построитель реестра: без JOIN и произвольных фрагментов запроса.
class QueryBuilder {
  public:
    QueryBuilder(const pg::Query& selection, Order order, bool descending,
        Field<std::string> identity = {"id", "id", "UUID", true})
        : order_(order), descending_(descending), identity_(identity) {
        statement_ = selection.GetStatementView();
        selection_name_ = selection.GetOptionalNameView().has_value()
            ? std::string{selection.GetOptionalNameView().value()}
            : "registry";
        for (const auto slot : {"{sort_key}", "{where}", "{order_by}", "{limit}"}) {
            const auto position = statement_.find(slot);
            if (position == std::string::npos ||
                statement_.find(slot, position + std::string_view{slot}.size()) !=
                    std::string::npos)
                throw std::logic_error{
                    "Каждое место SQL-шаблона должно встречаться ровно один раз"};
        }
        const auto date_slot = statement_.find("{date_key}");
        if ((order_.CursorType == CursorKeyType::Date && date_slot == std::string::npos) ||
            (date_slot != std::string::npos &&
                statement_.find("{date_key}", date_slot + 10) != std::string::npos))
            throw std::logic_error{"Некорректное место календарного ключа SQL-шаблона"};
    }

    template <typename T> void WhereEqual(Field<T> field, const T& value) {
        AddCondition(field.Expression, " = " + Bind(value, field.PgType));
    }

    template <typename T> void WhereEqual(Field<T> field, const std::optional<T>& value) {
        if (value.has_value())
            WhereEqual(field, value.value());
    }

    template <typename T>
    void WhereAny(Field<T> field, const std::optional<std::vector<T>>& values) {
        if (values.has_value())
            AddCondition(field.Expression,
                " = ANY(" + Bind(values.value(), std::string{field.PgType} + "[]") + ")");
    }

    void WhereContains(Field<std::string> field, const std::optional<std::string>& value) {
        const auto pattern = platform::registry::SearchPattern(value);
        if (pattern.has_value())
            AddCondition(field.Expression, " LIKE lower(" + Bind(pattern.value(), "TEXT") + ")");
    }

    template <typename T> void WhereAtLeast(Field<T> field, const std::optional<T>& value) {
        if (value.has_value())
            AddCondition(field.Expression, " >= " + Bind(value.value(), field.PgType));
    }

    template <typename T> void WhereAtMost(Field<T> field, const std::optional<T>& value) {
        if (value.has_value())
            AddCondition(field.Expression, " <= " + Bind(value.value(), field.PgType));
    }

    template <typename T> void WhereArrayContains(Field<T> field, const std::optional<T>& value) {
        if (value.has_value())
            AddCondition(
                Bind(value.value(), field.PgType), " = ANY(" + std::string{field.Expression} + ")");
    }

    template <typename T> void After(const std::optional<std::string>& id, const T& key) {
        if (!id.has_value())
            return;
        const std::string comparison = descending_ ? " < " : " > ";
        if (order_.ById) {
            AddCondition(identity_.Expression, comparison + Bind(id.value(), identity_.PgType));
        } else {
            // Параметр наследует COLLATE левого выражения. Принудительный C
            // переопределил бы выбранный полем порядок (например, числовой).
            const auto key_parameter = Bind(key, order_.PgType);
            const auto id_parameter = Bind(id.value(), identity_.PgType);
            AddCondition("(" + std::string{order_.Expression} + ", " +
                    std::string{identity_.Expression} + ")",
                comparison + "(" + key_parameter + ", " + id_parameter + ")");
        }
    }

    // Поле и выражение выбираются только из серверного каталога, значения — параметры.
    void WhereValues(FieldDefinition field, const std::vector<std::string>& values,
        bool fold = false, bool array = false) {
        std::vector<std::string> present;
        bool empty = false;
        for (const auto& value : values) {
            if (value.empty())
                empty = true;
            else
                present.push_back(value);
        }
        std::string condition;
        if (!present.empty()) {
            auto parameter = Bind(present, std::string{field.PgType} + "[]");
            if (fold)
                parameter = "ARRAY(SELECT lower(value) FROM unnest(" + parameter + ") AS value)";
            condition = std::string{field.Expression} + (array ? " && " : " = ANY(") + parameter +
                (array ? "" : ")");
        }
        if (empty) {
            if (!condition.empty())
                condition += " OR ";
            condition += array
                ? "COALESCE(cardinality(" + std::string{field.Expression} + "), 0) = 0"
                : field.PgType == "TEXT" ? std::string{field.Expression} + " = ''"
                                         : std::string{field.Expression} + " IS NULL";
        }
        AddCondition("", condition.empty() ? "FALSE" : "(" + condition + ")");
    }

    void WhereEmpty(FieldDefinition field, bool empty, bool array = false) {
        if (array)
            AddCondition("COALESCE(cardinality(" + std::string{field.Expression} + "), 0)",
                empty ? " = 0" : " <> 0");
        else
            AddCondition(field.Expression,
                field.PgType == "TEXT" ? (empty ? " = ''" : " <> ''")
                                       : (empty ? " IS NULL" : " IS NOT NULL"));
    }

    BuiltQuery BuildValues(const pg::Query& selection, std::string_view key, std::string_view label,
        std::string_view join, const std::optional<std::string>& after, int limit,
        std::string_view key_type = "TEXT") && {
        if (after.has_value())
            AddCondition(key, " > " + Bind(after.value(), key_type));
        const auto limit_parameter = Bind(limit, "INTEGER");
        auto statement = fmt::format(fmt::runtime(selection.GetStatementView()),
            fmt::arg("key", key), fmt::arg("label", label), fmt::arg("facet_join", join),
            fmt::arg("where", conditions_.empty() ? "TRUE" : conditions_),
            fmt::arg("limit", limit_parameter));
        return {pg::Query{std::move(statement), pg::Query::Name{"registry_filter_values"}},
            std::move(parameters_)};
    }

    BuiltQuery Build(int limit) && {
        const std::string direction = descending_ ? " DESC" : " ASC";
        std::string order;
        if (!order_.ById)
            order = std::string{order_.Expression} + direction + ", ";
        order += std::string{identity_.Expression} + direction;
        const auto limit_parameter = Bind(limit, "INTEGER");
        auto statement = fmt::format(fmt::runtime(statement_),
            fmt::arg("sort_key",
                (order_.ById || order_.CursorType == CursorKeyType::Date) ? "'' COLLATE \"C\""
                                                                          : order_.Expression),
            fmt::arg("date_key",
                order_.CursorType == CursorKeyType::Date ? order_.Expression : "NULL::DATE"),
            fmt::arg("where", conditions_.empty() ? "TRUE" : conditions_),
            fmt::arg("order_by", order), fmt::arg("limit", limit_parameter));
        return {pg::Query{std::move(statement), pg::Query::Name{std::string{selection_name_}}},
            std::move(parameters_)};
    }

  private:
    template <typename T> std::string Bind(const T& value, std::string_view type) {
        parameters_.PushBack(value);
        return "$" + std::to_string(parameters_.Size()) + "::" + std::string{type};
    }

    void AddCondition(std::string_view expression, const std::string& condition) {
        conditions_ += conditions_.empty() ? "" : "\n  AND ";
        conditions_ += expression;
        conditions_ += condition;
    }

    Order order_;
    bool descending_;
    Field<std::string> identity_;
    std::string statement_;
    std::string selection_name_;
    std::string conditions_;
    pg::ParameterStore parameters_;
};
}  // namespace platform::registry
