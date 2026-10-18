#pragma once

#include <platform/telemetry.hpp>
#include <userver/components/component_context.hpp>
#include <userver/ugrpc/server/middlewares/base.hpp>

namespace platform {

class GrpcTelemetry final : public userver::ugrpc::server::MiddlewareBase {
  public:
    explicit GrpcTelemetry(ServiceMetadata metadata) : metadata_(std::move(metadata)) {}

    void OnCallStart(userver::ugrpc::server::MiddlewareCallContext& context) const override {
        metadata_.Apply(context.GetSpan());
    }

  private:
    const ServiceMetadata metadata_;
};

class GrpcTelemetryFactory final : public userver::ugrpc::server::MiddlewareFactoryComponentBase {
  public:
    static constexpr std::string_view kName = "platform-grpc-telemetry";

    GrpcTelemetryFactory(const userver::components::ComponentConfig& config,
        const userver::components::ComponentContext& context)
        : MiddlewareFactoryComponentBase(config, context,
              userver::middlewares::MiddlewareDependencyBuilder()
                  .InGroup<userver::middlewares::groups::PreCore>()),
          metadata_(context.FindComponent<HttpTelemetryFactory>().GetMetadata()) {}

    std::shared_ptr<const userver::ugrpc::server::MiddlewareBase> CreateMiddleware(
        const userver::ugrpc::server::ServiceInfo&,
        const userver::yaml_config::YamlConfig&) const override {
        return std::make_shared<GrpcTelemetry>(metadata_);
    }

  private:
    const ServiceMetadata metadata_;
};

}  // namespace platform

template <>
inline constexpr bool userver::components::kHasValidate<platform::GrpcTelemetryFactory> = true;
