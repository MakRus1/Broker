#pragma once
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <cstdint>
#include <optional>
#include <string>
#include <userver/storages/postgres/io/composite_types.hpp>
#include <userver/storages/postgres/io/uuid.hpp>

namespace finance::models::pg {
struct FinancialLabelRaw {
    boost::uuids::uuid Id{};
    std::string Name{};
    std::string Direction{};
    std::string Unit{};
    std::string Currency{};
    std::string VatMode{};
    std::optional<std::int32_t> VatRateBasisPoints{};
    std::optional<std::string> Note{};
    bool Archived{};
    std::int64_t Revision{};
};
}  // namespace finance::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::finance::models::pg::FinancialLabelRaw> {
    static constexpr DBTypeName postgres_name = "public.financial_label_v1";
};

}  // namespace userver::storages::postgres::io
