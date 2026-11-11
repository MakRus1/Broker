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
struct PaymentRaw {
    boost::uuids::uuid Id{};
    std::string Kind{};
    std::string Number{};
    userver::utils::datetime::Date PaymentDate{};
    boost::uuids::uuid OwnCompanyId{};
    std::string OwnCompanyName{};
    boost::uuids::uuid CounterpartyId{};
    std::string CounterpartyName{};
    std::optional<boost::uuids::uuid> OwnBankAccountId{};
    std::optional<boost::uuids::uuid> CounterpartyBankAccountId{};
    std::string Currency{};
    std::int64_t AmountMinor{};
    std::optional<std::string> Purpose{};
    std::optional<std::string> Note{};
    bool Archived{};
    std::int64_t Revision{};
};
}  // namespace finance::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::finance::models::pg::PaymentRaw> {
    static constexpr DBTypeName postgres_name = "public.payment_v1";
};

}  // namespace userver::storages::postgres::io
