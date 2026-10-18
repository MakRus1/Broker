#pragma once

#include <platform/grpc_telemetry.hpp>
#include <platform/readiness.hpp>
#include <userver/ugrpc/server/middlewares/base.hpp>

namespace platform {

class GrpcReadiness final : public userver::ugrpc::server::MiddlewareBase {
  public:
    explicit GrpcReadiness(const Readiness& readiness) : readiness_(readiness) {}

    void OnCallStart(userver::ugrpc::server::MiddlewareCallContext& context) const override {
        // Проверяем новый RPC; уже принятые вызовы и потоки не прерываем.
        if (!readiness_.GetStatus().Ready)
            context.SetError(grpc::Status{grpc::StatusCode::UNAVAILABLE, "dependency unavailable"});
    }

  private:
    const Readiness& readiness_;
};

class GrpcReadinessFactory final : public userver::ugrpc::server::MiddlewareFactoryComponentBase {
  public:
    static constexpr std::string_view kName = "platform-grpc-readiness";

    GrpcReadinessFactory(const userver::components::ComponentConfig& config,
                         const userver::components::ComponentContext& context)
        : MiddlewareFactoryComponentBase(config, context,
                                         userver::middlewares::MiddlewareDependencyBuilder()
                                             .InGroup<userver::middlewares::groups::PreCore>()
                                             .After<GrpcTelemetryFactory>()),
          readiness_(context.FindComponent<Readiness>()) {}

    std::shared_ptr<const userver::ugrpc::server::MiddlewareBase>
    CreateMiddleware(const userver::ugrpc::server::ServiceInfo&,
                     const userver::yaml_config::YamlConfig&) const override {
        return std::make_shared<GrpcReadiness>(readiness_);
    }

  private:
    const Readiness& readiness_;
};

}  // namespace platform

template <>
inline constexpr bool userver::components::kHasValidate<platform::GrpcReadinessFactory> = true;
