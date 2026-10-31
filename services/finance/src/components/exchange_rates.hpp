#pragma once
#include <userver/clients/http/client.hpp>
#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include <userver/utils/datetime/date.hpp>

namespace finance {
struct Rate {
    std::int64_t Millionths;
    userver::utils::datetime::Date Date;
    std::string Source;
};

class ExchangeRates final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "exchange-rates";
    ExchangeRates(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();
    Rate Get(
        const std::string& from, const std::string& to, userver::utils::datetime::Date date) const;

  private:
    void EnsureDay(userver::utils::datetime::Date date) const;
    userver::clients::http::Client& http_;
    userver::storages::postgres::ClusterPtr pg_;
    std::string endpoint_;
    std::chrono::milliseconds timeout_;
};
}  // namespace finance

template <> inline constexpr bool userver::components::kHasValidate<finance::ExchangeRates> = true;
