#pragma once
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <userver/storages/postgres/io/composite_types.hpp>
#include <userver/storages/postgres/io/date.hpp>
#include <userver/storages/postgres/io/uuid.hpp>

namespace contracts::models::pg {
struct ContractRaw {
    boost::uuids::uuid Id{};
    boost::uuids::uuid OwnCompanyId{};
    boost::uuids::uuid CounterpartyId{};
    std::string Number{};
    userver::utils::datetime::Date ContractDate{};
    std::optional<userver::utils::datetime::Date> ValidUntil{};
    std::string Currency{};
    std::optional<boost::uuids::uuid> OwnBankAccountId{};
    std::optional<boost::uuids::uuid> CounterpartyBankAccountId{};
    std::optional<std::int32_t> PaymentDelayDays{};
    std::optional<std::string> Note{};
    bool Archived{};
    std::int64_t Revision{};
};
}  // namespace contracts::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::contracts::models::pg::ContractRaw> {
    static constexpr DBTypeName postgres_name = "public.contract_v1";
};

}  // namespace userver::storages::postgres::io
