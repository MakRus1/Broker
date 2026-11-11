#pragma once
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <userver/storages/postgres/io/array_types.hpp>
#include <userver/storages/postgres/io/composite_types.hpp>
#include <userver/storages/postgres/io/json_types.hpp>
#include <userver/storages/postgres/io/uuid.hpp>
#include <vector>

namespace counterparties::models::pg {
struct CounterpartyRaw {
    boost::uuids::uuid Id{};
    std::string Name{};
    std::int64_t Revision{};
    bool Archived{};
    std::optional<std::string> LegalName{};
    std::optional<std::string> CountryCode{};
    std::optional<std::string> TaxId{};
    std::optional<std::string> RegistrationId{};
    std::optional<std::string> LegalAddress{};
    std::vector<std::string> Roles{};
    userver::formats::json::Value Contacts{};
    std::optional<std::string> Kpp{};
    std::optional<std::string> Logist{};
    bool OwnCompany{};
};
}  // namespace counterparties::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::counterparties::models::pg::CounterpartyRaw> {
    static constexpr DBTypeName postgres_name = "public.counterparty_v6";
};

}  // namespace userver::storages::postgres::io
