#pragma once
#include <boost/uuid/uuid.hpp>
#include <string>
#include <userver/utils/datetime/date.hpp>

namespace finance::models {
struct ServiceChargeInvoiceRow {
    boost::uuids::uuid ServiceChargeId;
    boost::uuids::uuid InvoiceId;
    std::string Number;
    std::string Direction;
    userver::utils::datetime::Date InvoiceDate;
    bool ReadOnly;
};
}  // namespace finance::models
