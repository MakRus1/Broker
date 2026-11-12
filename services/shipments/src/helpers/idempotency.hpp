#pragma once
#include <helpers/database.hpp>
#include <helpers/json.hpp>
#include <models/pg/idempotent_result.hpp>
#include <openapi/models.hpp>

namespace shipments::helpers {
template <typename Result, typename Request>
std::optional<Result> ReadReplay(const pg::ResultSet& rows, const Request& request) {
    if (rows.IsEmpty())
        return std::nullopt;
    const auto saved = rows.template AsSingleRow<models::pg::IdempotentResult>();
    if (saved.Request != Document(request))
        throw ApiError<409>{"idempotency_conflict", "Ключ уже использован с другим запросом"};
    return models::ParseModelJson(saved.Result, std::type_identity<Result>{});
}

template <typename Result, typename Request>
std::optional<Result> FindReplay(pg::Transaction& transaction, const std::string& organization,
    const std::string& action, const Request& request) {
    transaction.Execute(sql::kLockIdempotency, organization, action, request.idempotency_key);
    const auto rows =
        transaction.Execute(sql::kFindIdempotency, organization, action, request.idempotency_key);
    return ReadReplay<Result>(rows, request);
}

// Быстрый повтор не зависит от доступности внешних справочников. Запись и гонки
// по-прежнему защищены FindReplay внутри транзакции создания.
template <typename Result, typename Request>
std::optional<Result> FindSavedReplay(const pg::ClusterPtr& cluster,
    const std::string& organization, const std::string& action, const Request& request) {
    const auto rows = cluster->Execute(pg::ClusterHostType::kMaster, sql::kFindIdempotency,
        organization, action, request.idempotency_key);
    return ReadReplay<Result>(rows, request);
}

template <typename Request, typename Result>
void SaveReplay(pg::Transaction& transaction, const std::string& organization,
    const std::string& action, const Request& request, const Result& result) {
    transaction.Execute(sql::kSaveIdempotency, organization, action, request.idempotency_key,
        Document(request), Document(result));
}
}  // namespace shipments::helpers
