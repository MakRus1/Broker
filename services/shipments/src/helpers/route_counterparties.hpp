#pragma once
#include <deps.hpp>
#include <helpers/database.hpp>
#include <platform/auth/identity.hpp>
#include <platform/http_client.hpp>
#include <set>

namespace shipments::helpers {
inline std::set<std::string> RouteCounterparties(
    const std::vector<models::ShipmentRouteSegment>& stages) {
    std::set<std::string> ids;
    const auto add = [&](const auto& id) {
        if (id.has_value())
            ids.insert(id.value());
    };
    for (const auto& stage : stages) {
        if (stage.sea.has_value()) {
            add(stage.sea.value().departure_terminal_id);
            add(stage.sea.value().arrival_terminal_id);
        }
        if (stage.air.has_value())
            add(stage.air.value().carrier_id);
    }
    return ids;
}

inline void ValidateRouteCounterparties(const Deps& deps,
    const userver::server::request::RequestContext& context,
    const std::vector<models::ShipmentRouteSegment>& stages,
    const std::vector<models::ShipmentRouteSegment>& previous) {
    const auto old = RouteCounterparties(previous);
    for (const auto& id : RouteCounterparties(stages)) {
        if (old.contains(id))
            continue;
        try {
            const auto response = deps.counterparties.ApiV1CounterpartiesGet(
                {.body = {.id = id}}, platform::auth::GetAccessToken(context));
            if (response.counterparty.archived)
                throw ApiError{400, "invalid_route_counterparty",
                    "Выберите действующего контрагента организации"};
        } catch (const platform::http::ApiError& error) {
            if (error.GetStatus() == 404)
                throw ApiError{
                    400, "invalid_route_counterparty", "Контрагент не найден в организации"};
            throw ApiError{
                503, "counterparty_unavailable", "Не удалось проверить контрагента участка"};
        } catch (const platform::http::CancelledError&) {
            throw;
        } catch (const platform::http::TransportError&) {
            throw ApiError{
                503, "counterparty_unavailable", "Не удалось проверить контрагента участка"};
        } catch (const platform::http::ContractError&) {
            throw ApiError{
                503, "counterparty_unavailable", "Некорректный ответ справочника контрагентов"};
        }
    }
}
}  // namespace shipments::helpers
