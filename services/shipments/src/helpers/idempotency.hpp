#pragma once
#include <helpers/database.hpp>
#include <helpers/json.hpp>
#include <openapi/models.hpp>
#include <postgresql/types.hpp>

namespace shipments::helpers {
template <typename Result, typename Request>
std::optional<Result> FindReplay(pg::Transaction& transaction, const std::string& tenant,
    const std::string& action, const Request& request) {
    transaction.Execute(sql::kLockIdempotency, tenant, action, request.idempotency_key);
    const auto rows =
        transaction.Execute(sql::kFindIdempotency, tenant, action, request.idempotency_key);
    if (rows.IsEmpty())
        return std::nullopt;
    const auto saved = rows.template AsSingleRow<pgtypes::IdempotentResultV1>();
    if (saved.Request != Document(request))
        throw ApiError{409, "idempotency_conflict", "Ключ уже использован с другим запросом"};
    return models::ParseModelJson(saved.Result, std::type_identity<Result>{});
}

template <typename Request, typename Result>
void SaveReplay(pg::Transaction& transaction, const std::string& tenant, const std::string& action,
    const Request& request, const Result& result) {
    transaction.Execute(sql::kSaveIdempotency, tenant, action, request.idempotency_key,
        Document(request), Document(result));
}
}  // namespace shipments::helpers
