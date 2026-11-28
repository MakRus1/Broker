#pragma once
#include <clients/counterparties/client.hpp>
#include <platform/http_client.hpp>
#include <userver/logging/log.hpp>
#include <userver/utils/boost_uuid4.hpp>

#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace web_bff::clients::counterparties {
// Контрагенты по ID одной страницы реестра. Имя клиента в реестре необязательно: без права чтения
// или при сбое сервиса контрагентов список пуст.
inline std::vector<models::Counterparty> ListByIds(const Client& client,
    const std::set<boost::uuids::uuid>& ids, std::string_view access_token) try {
    if (ids.empty())
        return {};
    std::vector<std::string> values;
    for (const auto& id : ids)
        values.push_back(::userver::utils::ToString(id));
    std::vector<models::RegistryFilter> filters{{.field = "id", .values = std::move(values)}};
    return client
        .ApiV1CounterpartiesList({.body = {.filters = std::move(filters),
                                      .limit = static_cast<std::int64_t>(ids.size())}},
            access_token)
        .items;
} catch (const ::platform::http::CancelledError&) {

    throw;
} catch (const ::platform::http::ErrorByStatus<403>&) {
    return {};
} catch (const ::platform::http::ApiError& error) {
    LOG_WARNING() << "counterparties unavailable for registry, HTTP " << error.GetStatus();
    return {};
} catch (const ::platform::http::TransportError& error) {
    LOG_WARNING() << "counterparties unavailable for registry: " << error.what();
    return {};
} catch (const ::platform::http::ContractError& error) {
    LOG_WARNING() << "counterparties invalid response for registry: " << error.what();
    return {};
}
}  // namespace web_bff::clients::counterparties
