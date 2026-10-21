#pragma once
#include <deps.hpp>
#include <userver/server/request/request_context.hpp>
#include <views/api/v1/transportation_operations/create/requests.hpp>
#include <views/api/v1/transportation_operations/create/responses.hpp>

namespace shipments::views::api::v1::transportation_operations::create {
class View {
  public:
    static Response Handle(
        Request&& request, userver::server::request::RequestContext& context, Deps deps);
};
}  // namespace shipments::views::api::v1::transportation_operations::create
