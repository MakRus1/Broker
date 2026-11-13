#pragma once
#include <boost/uuid/uuid.hpp>
#include <optional>
#include <string>
#include <userver/utils/datetime/date.hpp>

namespace finance::models {
struct ReconciliationRow {
    bool IsTotal;
    std::string Currency;
    std::optional<boost::uuids::uuid> DocumentId;
    std::optional<userver::utils::datetime::Date> DocumentDate;
    std::optional<std::string> Kind;
    std::string Number;
    std::string OwnCompanyName;
    std::string DebitMinor;
    std::string CreditMinor;
    std::string BalanceMinor;
    std::string OpeningMinor;
};
}  // namespace finance::models
