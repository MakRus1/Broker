#pragma once
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <userver/storages/postgres/io/composite_types.hpp>
#include <userver/storages/postgres/io/date.hpp>
#include <userver/storages/postgres/io/uuid.hpp>

namespace finance::models::pg {
struct ServiceChargeRaw {
    boost::uuids::uuid Id{};
    std::optional<boost::uuids::uuid> ShipmentId{};
    std::optional<boost::uuids::uuid> TransportationId{};
    boost::uuids::uuid LabelId{};
    std::string LabelName{};
    std::string Direction{};
    std::string Unit{};
    boost::uuids::uuid CounterpartyId{};
    std::string CounterpartyName{};
    std::optional<boost::uuids::uuid> ContractId{};
    std::optional<std::string> ContractNumber{};
    std::int64_t QuantityMilli{};
    std::int64_t PriceMinor{};
    std::string Currency{};
    std::string VatMode{};
    std::optional<std::int32_t> VatRateBasisPoints{};
    std::int64_t NetMinor{};
    std::int64_t VatMinor{};
    std::int64_t TotalMinor{};
    std::optional<std::string> Note{};
    bool Archived{};
    std::int64_t Revision{};
    std::string PriceMode{};
    userver::utils::datetime::Date ServiceDate{};
    std::string InvoiceCurrency{};
    std::int64_t InvoiceRateMillionths{};
    userver::utils::datetime::Date InvoiceRateDate{};
    std::string InvoiceRateSource{};
    std::int64_t InvoiceNetMinor{};
    std::int64_t InvoiceVatMinor{};
    std::int64_t InvoiceTotalMinor{};
};
}  // namespace finance::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::finance::models::pg::ServiceChargeRaw> {
    static constexpr DBTypeName postgres_name = "public.service_charge_v1";
};

}  // namespace userver::storages::postgres::io
