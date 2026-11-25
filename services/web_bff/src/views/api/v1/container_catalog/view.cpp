#include <algorithm>
#include <views/api/v1/container_catalog/view.hpp>

namespace web_bff::views::api::v1::container_catalog {
Response View::Handle(Request&&, userver::server::request::RequestContext&, Deps deps) {
    const auto catalog = deps.configs.GetSnapshot().Get(configs::CONTAINER_CATALOG);
    Response200 response;
    for (auto group = catalog.begin(); group != catalog.end(); ++group) {
        const auto& value = *group;
        if (!value["active"].As<bool>())
            continue;
        response.items.push_back({.code = group.GetName(),
            .iso_codes = value["iso_codes"].As<std::vector<std::string>>(),
            .name = value["name"].As<std::string>()});
    }
    std::ranges::sort(response.items, {}, [](const auto& item) { return item.code; });
    return response;
}
}  // namespace web_bff::views::api::v1::container_catalog
