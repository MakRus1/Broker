#pragma once
#include <cstdint>
#include <string>
#include <userver/formats/json/value.hpp>
#include <userver/storages/postgres/io/composite_types.hpp>
#include <userver/storages/postgres/io/json_types.hpp>

namespace sales::models::pg {
struct SalesSettings {
    std::int64_t Revision{};
    userver::formats::json::Value Stages{};
    std::string InquiryNumberTemplate{};
    bool ResetNumberYearly{};
};
}  // namespace sales::models::pg

namespace userver::storages::postgres::io {

template <> struct CppToUserPg<::sales::models::pg::SalesSettings> {
    static constexpr DBTypeName postgres_name = "public.sales_settings_v1";
};

}  // namespace userver::storages::postgres::io
