#pragma once
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <userver/storages/postgres/io/composite_types.hpp>
#include <userver/storages/postgres/io/uuid.hpp>

namespace counterparties::models::pg {
struct BankAccountRaw {
    boost::uuids::uuid Id{};
    boost::uuids::uuid CounterpartyId{};
    std::string Name{};
    std::string Number{};
    std::string Currency{};
    std::string BankName{};
    std::optional<std::string> Bic{};
    std::optional<std::string> Swift{};
    std::optional<std::string> CorrespondentAccount{};
    std::optional<std::string> Iban{};
    bool IsPrimary{};
    bool Archived{};
    std::int64_t Revision{};
};
}  // namespace counterparties::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::counterparties::models::pg::BankAccountRaw> {
    static constexpr DBTypeName postgres_name = "public.bank_account_v1";
};

}  // namespace userver::storages::postgres::io
