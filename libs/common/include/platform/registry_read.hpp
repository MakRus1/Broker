#pragma once
#include <platform/db.hpp>
#include <platform/handler_error.hpp>
#include <platform/registry_page.hpp>
#include <platform/registry_query.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/storages/postgres/transaction.hpp>

namespace platform::registry {

inline pg::ResultSet ExecuteWithCustomPlan(pg::Transaction& transaction, const BuiltQuery& query) {
    // SET LOCAL ограничивает выбор плана текущей транзакцией.
    static const pg::Query plan{"SET LOCAL plan_cache_mode = 'force_custom_plan';",
        pg::Query::NameLiteral("registry_plan"), pg::Query::LogMode::kNameOnly};
    transaction.Execute(plan);
    return transaction.Execute(query.Statement, query.Parameters);
}

template <typename Row, typename Cursor> struct Page {
    std::vector<Row> Rows;
    std::optional<Cursor> NextCursor;
};

// Страница реестра целиком: проверка курсора, сборка запроса, чтение и курсор следующей
// страницы. Транзакция внутри нужна только для SET LOCAL и наружу не выходит.
// Ошибка фильтра превращается в 400 invalid_filter.
template <typename Row, typename Request, typename Context, typename GetId>
auto ReadPage(const pg::ClusterPtr& cluster, const Definition<Request, Context>& registry,
    const Request& request, const Context& context, GetId get_id) {
    using Cursor = typename std::remove_cvref_t<decltype(request.cursor)>::value_type;
    try {
        const auto scope = CursorScope(request, context);
        const auto order = RegistryOrderBy(registry.Fields, request.sort);
        bool date_sort = false;
        if constexpr (requires(Cursor cursor) { cursor.date_value; })
            date_sort = order.CursorType == CursorKeyType::Date;
        if constexpr (requires { request.after; }) {
            if (request.after.has_value() &&
                (request.cursor.has_value() || !order.ById || request.descending))
                throw platform::http::HandlerError<400>{
                    "invalid_cursor", "Для выбранной сортировки используйте cursor"};
        }
        ValidateListCursor(request.cursor, scope, date_sort);

        const auto query = BuildPage(registry, request, context);
        auto transaction = cluster->Begin(pg::ClusterHostType::kMaster, db::kReadOnlySnapshot);
        auto rows = ExecuteWithCustomPlan(transaction, query)
                        .template AsContainer<std::vector<Row>>(pg::kRowTag);
        transaction.Commit();
        auto next = FinishListPage<Cursor>(rows, request.limit, scope, get_id, date_sort);
        return Page<Row, Cursor>{.Rows = std::move(rows), .NextCursor = std::move(next)};
    } catch (const FilterError& error) {
        throw platform::http::HandlerError<400>{"invalid_filter", error.what()};
    }
}

}  // namespace platform::registry
