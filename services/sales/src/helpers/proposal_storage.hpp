#pragma once
#include <helpers/database.hpp>

namespace sales::helpers {
inline models::Proposal ReadProposal(const pg::ResultSet& rows) {
    if (rows.IsEmpty())
        throw ApiError<404>{"proposal_not_found", "Предложение не найдено"};
    return models::ParseModelJson(
        rows.AsSingleRow<json::Value>(), std::type_identity<models::Proposal>{});
}

}  // namespace sales::helpers
