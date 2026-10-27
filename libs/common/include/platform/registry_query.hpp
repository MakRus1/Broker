#pragma once

#include <array>
#include <fmt/format.h>
#include <optional>
#include <platform/registry.hpp>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <userver/storages/postgres/parameter_store.hpp>
#include <userver/storages/postgres/query.hpp>
#include <vector>

namespace platform::registry {
namespace pg = userver::storages::postgres;

// SQL-выражения задаёт разработчик. Значения запроса всегда идут через ParameterStore.
struct FieldDefinition {
    std::string_view Name;
    std::string_view Expression;
    std::string_view PgType = "TEXT";
    bool ById = false;
    std::string_view SortExpression;
    std::string_view SortPgType;
};

template <typename T> struct Field : FieldDefinition {
    constexpr Field(std::string_view name, std::string_view expression,
        std::string_view pg_type = "TEXT", bool by_id = false)
        : FieldDefinition{name, expression, pg_type, by_id, {}, {}} {}

    constexpr Field SortedAs(std::string_view expression, std::string_view pg_type = "TEXT") const {
        auto field = *this;
        field.SortExpression = expression;
        field.SortPgType = pg_type;
        return field;
    }
};

struct Order {
    std::string_view Expression;
    bool ById = false;
    std::string_view PgType = "TEXT";
};

inline Order OrderBy(std::span<const FieldDefinition> fields, std::string_view name) {
    for (const auto& field : fields) {
        if (field.Name == name)
            return {field.SortExpression.empty() ? field.Expression : field.SortExpression,
                field.ById, field.SortPgType.empty() ? field.PgType : field.SortPgType};
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
        if ((order_.PgType == "DATE" && date_slot == std::string::npos) ||
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
            auto key_parameter = Bind(key, order_.PgType);
            if (order_.PgType == "TEXT")
                key_parameter += " COLLATE \"C\"";
            const auto id_parameter = Bind(id.value(), identity_.PgType);
            AddCondition("(" + std::string{order_.Expression} + ", " +
                    std::string{identity_.Expression} + ")",
                comparison + "(" + key_parameter + ", " + id_parameter + ")");
        }
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
                (order_.ById || order_.PgType == "DATE") ? "'' COLLATE \"C\"" : order_.Expression),
            fmt::arg("date_key", order_.PgType == "DATE" ? order_.Expression : "NULL::DATE"),
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
