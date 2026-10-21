#pragma once
#include <helpers/database.hpp>
#include <postgresql/types.hpp>

namespace shipments::helpers {
inline pgtypes::QuantityTotalsV1 ReadTotals(
    pg::Transaction& transaction, const std::string& organization, const std::string& item) {
    return transaction.Execute(sql::kOperationTotals, organization, item)
        .AsSingleRow<pgtypes::QuantityTotalsV1>();
}

}  // namespace shipments::helpers
