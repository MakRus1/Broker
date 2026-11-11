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
struct InvoiceRaw {
    boost::uuids::uuid Id{};
    std::string Direction{};
    std::string Number{};
    userver::utils::datetime::Date InvoiceDate{};
    std::optional<userver::utils::datetime::Date> DueDate{};
    boost::uuids::uuid OwnCompanyId{};
    std::string OwnCompanyName{};
    boost::uuids::uuid CounterpartyId{};
    std::string CounterpartyName{};
    std::optional<boost::uuids::uuid> OwnBankAccountId{};
    std::optional<boost::uuids::uuid> CounterpartyBankAccountId{};
    std::optional<boost::uuids::uuid> ContractId{};
    std::optional<std::string> ContractNumber{};
    std::string Currency{};
    std::optional<std::string> Note{};
    bool Archived{};
    std::int64_t Revision{};
    std::int64_t NetMinor{};
    std::int64_t VatMinor{};
    std::int64_t TotalMinor{};
};
}  // namespace finance::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::finance::models::pg::InvoiceRaw> {
    static constexpr DBTypeName postgres_name = "public.invoice_v1";
};

}  // namespace userver::storages::postgres::io
