#pragma once
#include <helpers/database.hpp>

namespace sales::helpers {
inline models::Inquiry LockEditableInquiry(
    pg::Transaction& transaction, const std::string& organization, const std::string& id) {
    const auto rows = transaction.Execute(sql::kLockInquiries, organization, id);
    if (rows.IsEmpty())
        throw ApiError<404>{"not_found", "Входящий запрос не найден"};
    auto inquiry = models::ParseModelJson(
        rows.AsSingleRow<json::Value>(), std::type_identity<models::Inquiry>{});
    if (inquiry.converting || inquiry.shipment_id)
        throw ApiError<409>{"conversion_started", "Запрос уже передан в поставку"};
    return inquiry;
}

}  // namespace sales::helpers
