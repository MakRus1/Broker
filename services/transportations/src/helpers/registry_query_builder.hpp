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

namespace transportations::helpers::registry {
namespace pg = userver::storages::postgres;

// SQL-выражения задаёт разработчик. Значения запроса всегда идут через ParameterStore.
template <typename T> struct Field {
    std::string_view Name;
    std::string_view Expression;
    std::string_view PgType = "TEXT";
    bool ById = false;
};

struct Order {
    std::string_view Expression;
    bool ById = false;
};

inline Order OrderBy(std::span<const Field<std::string>> fields, std::string_view name) {
    for (const auto& field : fields) {
        if (field.Name == name)
            return {field.Expression, field.ById};
    }
    throw std::logic_error{"Поле не зарегистрировано для сортировки"};
}

struct BuiltQuery {
    pg::Query Statement;
    pg::ParameterStore Parameters;
};

// Прототип для одного реестра: без JOIN, произвольных фрагментов и языка выражений.
class QueryBuilder {
  public:
    QueryBuilder(const pg::Query& selection, Order order, bool descending)
        : order_(order), descending_(descending) {
        statement_ = selection.GetStatementView();
        for (const auto slot : {"{sort_key}", "{where}", "{order_by}", "{limit}"}) {
            const auto position = statement_.find(slot);
            if (position == std::string::npos ||
                statement_.find(slot, position + std::string_view{slot}.size()) !=
                    std::string::npos)
                throw std::logic_error{
                    "Каждое место SQL-шаблона должно встречаться ровно один раз"};
        }
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
        std::string order;
        if (!order_.ById)
            order = std::string{order_.Expression} + direction + ", ";
        order += "id" + direction;
        const auto limit_parameter = Bind(limit, "INTEGER");
        auto statement = fmt::format(fmt::runtime(statement_),
            fmt::arg("sort_key", order_.ById ? "'' COLLATE \"C\"" : order_.Expression),
            fmt::arg("where", conditions_), fmt::arg("order_by", order),
            fmt::arg("limit", limit_parameter));
        return {pg::Query{std::move(statement), pg::Query::Name{"transportations_registry"}},
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
    std::string statement_;
    std::string conditions_;
    pg::ParameterStore parameters_;
};
}  // namespace transportations::helpers::registry
