#pragma once
#include <platform/registry_query.hpp>
#include <userver/storages/postgres/transaction.hpp>

namespace platform::registry {

inline pg::ResultSet ExecuteWithCustomPlan(pg::Transaction& transaction, const BuiltQuery& query) {
    // SET LOCAL ограничивает выбор плана текущей транзакцией.
    static const pg::Query plan{"SET LOCAL plan_cache_mode = 'force_custom_plan';",
        pg::Query::NameLiteral("registry_plan"), pg::Query::LogMode::kNameOnly};
    transaction.Execute(plan);
    return transaction.Execute(query.Statement, query.Parameters);
}

}  // namespace platform::registry
