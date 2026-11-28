#pragma once
#include <helpers/database.hpp>
#include <helpers/json.hpp>
#include <models/pg/idempotent_result.hpp>
#include <openapi/models.hpp>
#include <platform/db.hpp>

namespace shipments::helpers {
template <typename Result, typename Request>
std::optional<Result> ReadReplay(const pg::ResultSet& rows, const Request& request) {
    if (rows.IsEmpty())
        return std::nullopt;
    const auto saved = rows.template AsSingleRow<models::pg::IdempotentResult>();
    const auto document = Document(request);
    // Ранее сохранённый запрос может содержать UUID в верхнем регистре.
    if (saved.Request != document &&
        Document(models::ParseModelJson(saved.Request, std::type_identity<Request>{})) != document)
        throw ApiError<409>{"idempotency_conflict", "Ключ уже использован с другим запросом"};
    return models::ParseModelJson(saved.Result, std::type_identity<Result>{});
}

// Повтор без блокировки годится для быстрого ответа до транзакции. Внутри транзакции записи
// сначала берётся LockReplay, чтобы параллельный запрос с тем же ключом дождался результата.
template <typename Request>
void LockReplay(pg::Transaction& transaction, const boost::uuids::uuid& organization,
    const std::string& action, const Request& request) {
    transaction.Execute(sql::kLockIdempotency, organization, action, request.idempotency_key);
}

template <typename Result, typename Request>
std::optional<Result> FindReplay(platform::db::Executor executor,
    const boost::uuids::uuid& organization, const std::string& action, const Request& request) {
    return ReadReplay<Result>(
        executor.Execute(sql::kFindIdempotency, organization, action, request.idempotency_key),
        request);
}

template <typename Request, typename Result>
void SaveReplay(pg::Transaction& transaction, const boost::uuids::uuid& organization,
    const std::string& action, const Request& request, const Result& result) {
    transaction.Execute(sql::kSaveIdempotency, organization, action, request.idempotency_key,
        Document(request), Document(result));
}
}  // namespace shipments::helpers
