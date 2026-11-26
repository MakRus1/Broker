#pragma once
#include <deps.hpp>
#include <views/api/v1/container_catalog/requests.hpp>
#include <views/api/v1/container_catalog/responses.hpp>

namespace web_bff::views::api::v1::container_catalog {
struct View {
    static Response Handle(Request&&, userver::server::request::RequestContext&, Deps);
};
}  // namespace web_bff::views::api::v1::container_catalog
