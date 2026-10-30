#pragma once
#include <deps.hpp>
#include <helpers/database.hpp>
#include <platform/auth/identity.hpp>
#include <platform/http_client.hpp>
#include <set>

namespace transportations::helpers {
inline std::set<std::string> RouteCounterparties(const auto& stages) {
    std::set<std::string> ids;
    for (const auto& stage : stages) {
        if (stage.contractor_id.has_value())
            ids.insert(stage.contractor_id.value());
        if (stage.international_road.has_value() &&
            stage.international_road.value().loading_terminal_id.has_value())
            ids.insert(stage.international_road.value().loading_terminal_id.value());
    }
    return ids;
}

inline void ValidateRouteCounterparties(const Deps& deps,
    const userver::server::request::RequestContext& context,
    const std::vector<models::SetRouteSegmentsItem>& stages,
    const std::vector<models::RouteSegment>& previous) {
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
        } catch (const platform::http::ErrorByStatus<404>&) {
            throw ApiError{400, "invalid_route_counterparty", "Контрагент не найден в организации"};
        } catch (const platform::http::ApiError&) {
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
}  // namespace transportations::helpers
