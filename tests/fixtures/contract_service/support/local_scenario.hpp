#pragma once

#include <chrono>
#include <deps.hpp>
#include <handlers/hello_client.usrv.pb.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/logging/log.hpp>
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/tracing/span.hpp>
#include <userver/ugrpc/client/client_factory_component.hpp>

namespace contract_service {

// Только локальный стенд: одна трасса проходит через HTTP, PostgreSQL и gRPC.
class LocalScenario final : public userver::server::handlers::HttpHandlerBase {
  public:
    static constexpr std::string_view kName = "local-scenario";

    LocalScenario(const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context)
        : HttpHandlerBase(config, context), deps_(ResolveDeps(context)),
          factory_(context
                       .FindComponent<userver::ugrpc::client::ClientFactoryComponent>(
                           "scenario-grpc-factory")
                       .GetFactory()) {}

    std::string HandleRequestThrow(const userver::server::http::HttpRequest&,
        userver::server::request::RequestContext&) const override {
        const auto postgres = deps_.broker.HelloPostgres({.name = "local-scenario"});
        auto client =
            factory_.MakeClient<handlers::api::HelloServiceClient>("local-scenario", "broker:8081");
        handlers::api::HelloRequest request;
        request.set_name("local-scenario");
        userver::ugrpc::client::CallOptions options;
        options.SetTimeout(std::chrono::seconds{2});
        const auto grpc = client.SayHello(request, std::move(options));
        userver::formats::json::ValueBuilder result;
        result["postgres"] = postgres.body;
        result["grpc"] = grpc.text();
        result["trace_id"] = std::string(userver::tracing::Span::CurrentSpan().GetTraceId());
        LOG_INFO() << "Local scenario completed";
        return userver::formats::json::ToString(result.ExtractValue());
    }

  private:
    Deps deps_;
    userver::ugrpc::client::ClientFactory& factory_;
};

}  // namespace contract_service
