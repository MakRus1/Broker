#include <components/public_catalog.hpp>
#include <userver/server/http/http_request.hpp>
#include <userver/server/http/http_response.hpp>

namespace web_bff {
namespace {
class Cors final : public userver::server::middlewares::HttpMiddlewareBase {
    void HandleRequest(userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context) const override {
        if (request.GetRequestPath() == "/localization/catalog" && request.GetMethodStr() == "GET")
            request.GetHttpResponse().SetHeader(
                std::string_view{"Access-Control-Allow-Origin"}, "*");
        Next(request, context);
    }
};
}  // namespace

std::unique_ptr<userver::server::middlewares::HttpMiddlewareBase> PublicCatalogCors::Create(
    const userver::server::handlers::HttpHandlerBase&, userver::yaml_config::YamlConfig) const {
    return std::make_unique<Cors>();
}
}  // namespace web_bff
