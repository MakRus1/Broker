#pragma once

#include <userver/server/middlewares/http_middleware_base.hpp>

namespace web_bff {
class PublicCatalogCors final : public userver::server::middlewares::HttpMiddlewareFactoryBase {
  public:
    static constexpr std::string_view kName = "public-catalog-cors";
    using HttpMiddlewareFactoryBase::HttpMiddlewareFactoryBase;

  private:
    std::unique_ptr<userver::server::middlewares::HttpMiddlewareBase> Create(
        const userver::server::handlers::HttpHandlerBase&,
        userver::yaml_config::YamlConfig) const override;
};
}  // namespace web_bff
