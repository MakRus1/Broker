#pragma once

#include <optional>
#include <platform/registry.hpp>
#include <string>
#include <string_view>
#include <userver/storages/postgres/parameter_store.hpp>
#include <userver/storages/postgres/query.hpp>
#include <vector>

namespace transportations::helpers::registry {
namespace pg = userver::storages::postgres;

// SQL-выражения задаёт разработчик. Значения запроса всегда идут через ParameterStore.
template <typename T> struct Field {
    std::string_view Expression;
    std::string_view PgType;
};

struct Order {
    std::string_view Expression;
    bool ById = false;
};

struct BuiltQuery {
    pg::Query Statement;
    pg::ParameterStore Parameters;
};

// Прототип для одного реестра: без JOIN, произвольных фрагментов и языка выражений.
class QueryBuilder {
  public:
    QueryBuilder(const pg::Query& selection, Order order, bool descending)
        : order_(order), descending_(descending) {
        statement_ = "WITH registry AS (\n";
        statement_ += selection.GetStatementView();
        statement_ += "\n)\nSELECT transportation, ";
        statement_ += order_.Expression;
        statement_ += " AS sort_key\nFROM registry\nWHERE TRUE";
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

    void After(const std::optional<std::string>& id, const std::string& key) {
        if (!id.has_value())
            return;
        const std::string comparison = descending_ ? " < " : " > ";
        if (order_.ById) {
            AddCondition("id", comparison + Bind(id.value(), "UUID"));
        } else {
            const auto key_parameter = Bind(key, "TEXT");
            const auto id_parameter = Bind(id.value(), "UUID");
            AddCondition("(" + std::string{order_.Expression} + ", id)",
                comparison + "(" + key_parameter + " COLLATE \"C\", " + id_parameter + ")");
        }
    }

    BuiltQuery Build(int limit) && {
        const std::string direction = descending_ ? " DESC" : " ASC";
        statement_ += "\nORDER BY ";
        if (!order_.ById)
            statement_ += std::string{order_.Expression} + direction + ", ";
        statement_ += "id" + direction;
        statement_ += "\nLIMIT " + Bind(limit, "INTEGER");
        return {pg::Query{std::move(statement_), pg::Query::Name{"transportations_registry"}},
            std::move(parameters_)};
    }

  private:
    template <typename T> std::string Bind(const T& value, std::string_view type) {
        parameters_.PushBack(value);
        return "$" + std::to_string(parameters_.Size()) + "::" + std::string{type};
    }

    void AddCondition(std::string_view expression, const std::string& condition) {
        statement_ += "\n  AND ";
        statement_ += expression;
        statement_ += condition;
    }

    Order order_;
    bool descending_;
    std::string statement_;
    pg::ParameterStore parameters_;
};
}  // namespace transportations::helpers::registry
