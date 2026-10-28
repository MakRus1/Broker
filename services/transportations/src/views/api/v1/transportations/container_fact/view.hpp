#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/transportations/container_fact/requests.hpp>
#include <views/api/v1/transportations/container_fact/responses.hpp>

namespace transportations::views::api::v1::transportations::container_fact {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace transportations::views::api::v1::transportations::container_fact
