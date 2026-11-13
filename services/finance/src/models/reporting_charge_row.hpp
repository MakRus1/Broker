#pragma once
#include <boost/uuid/uuid.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <userver/utils/datetime/date.hpp>

namespace finance::models {
struct ReportingChargeRow {
    userver::utils::datetime::Date Date;
    std::string Currency;
    std::string InvoiceCurrency;
    std::int64_t TotalMinor;
    std::int64_t InvoiceTotalMinor;
    std::string Direction;
    std::optional<boost::uuids::uuid> TransportationId;
};
}  // namespace finance::models
