#include <components/sales_directory.hpp>
#include <map>
#include <models/errors.hpp>
#include <platform/charge_amounts.hpp>
#include <platform/http_client.hpp>
#include <set>
#include <userver/components/component_context.hpp>
#include <userver/utils/text_light.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

namespace sales {
namespace json = userver::formats::json;

SalesDirectory::SalesDirectory(const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      counterparties_(context.FindComponent<clients::counterparties::Component>().GetClient()),
      finance_(context.FindComponent<clients::finance::Component>().GetClient()),
      organizations_(context.FindComponent<clients::organizations::Component>().GetClient()),
      transport_locations_(
          context.FindComponent<clients::transport_locations::Component>().GetClient()) {}

userver::yaml_config::Schema SalesDirectory::GetStaticConfigSchema() {
    return ComponentBase::GetStaticConfigSchema();
}

clients::counterparties::models::Counterparty SalesDirectory::GetCounterparty(
    std::string_view token, const std::string& customer) const try {
    return counterparties_.ApiV1CounterpartiesGet({.body = {.id = customer}}, token).counterparty;
} catch (const platform::http::ErrorByStatus<403>&) {

    throw ApiError<403>{"reference_forbidden", "Нет доступа к связанным данным"};
} catch (const platform::http::ErrorByStatus<404>&) {
    throw ApiError<404>{"reference_not_found", "Связанный объект не найден в организации"};
} catch (const platform::http::CancelledError&) {
    throw;
} catch (const platform::http::ApiError&) {
    throw ApiError<502>{"reference_unavailable", "Не удалось получить связанные данные"};
} catch (const platform::http::TransportError&) {
    throw ApiError<502>{"reference_unavailable", "Сервис связанных данных недоступен"};
} catch (const platform::http::ContractError&) {
    throw ApiError<502>{"reference_unavailable", "Некорректный ответ сервиса связанных данных"};
}

void SalesDirectory::ValidateSalesParties(const std::string& organization, std::string_view token,
    const std::string& customer, const std::string& manager) const try {
    const auto party = GetCounterparty(token, customer);
    if (party.archived)
        throw ApiError<409>{"archived_customer", "Выберите действующего клиента"};
    const auto employees = organizations_.ApiOrganizationsEmployeesList(
        {.body = {.limit = 1, .organization_id = organization, .subject = manager}}, token);
    if (employees.items.empty())
        throw ApiError<400>{"invalid_manager", "Выберите действующего сотрудника организации"};
} catch (const platform::http::ErrorByStatus<403>&) {

    throw ApiError<403>{"reference_forbidden", "Нет доступа к связанным данным"};
} catch (const platform::http::ErrorByStatus<404>&) {
    throw ApiError<404>{"reference_not_found", "Связанный объект не найден в организации"};
} catch (const platform::http::CancelledError&) {
    throw;
} catch (const platform::http::ApiError&) {
    throw ApiError<502>{"reference_unavailable", "Не удалось получить связанные данные"};
} catch (const platform::http::TransportError&) {
    throw ApiError<502>{"reference_unavailable", "Сервис связанных данных недоступен"};
} catch (const platform::http::ContractError&) {
    throw ApiError<502>{"reference_unavailable", "Некорректный ответ сервиса связанных данных"};
}

void SalesDirectory::RequireAdministrator(
    const std::string& organization, std::string_view token) const try {
    organizations_.ApiOrganizationsAdministrationCheck(
        {.body = {.organization_id = organization}}, token);
} catch (const platform::http::ErrorByStatus<403>&) {

    throw ApiError<403>{"reference_forbidden", "Нет доступа к связанным данным"};
} catch (const platform::http::ErrorByStatus<404>&) {
    throw ApiError<404>{"reference_not_found", "Связанный объект не найден в организации"};
} catch (const platform::http::CancelledError&) {
    throw;
} catch (const platform::http::ApiError&) {
    throw ApiError<502>{"reference_unavailable", "Не удалось получить связанные данные"};
} catch (const platform::http::TransportError&) {
    throw ApiError<502>{"reference_unavailable", "Сервис связанных данных недоступен"};
} catch (const platform::http::ContractError&) {
    throw ApiError<502>{"reference_unavailable", "Некорректный ответ сервиса связанных данных"};
}

void SalesDirectory::ResolveShipmentRoute(const std::string& organization, std::string_view token,
    std::vector<models::ShipmentRouteSegment>& segments,
    const std::optional<std::string>& logist) const try {
    if (logist.has_value()) {
        const auto employees = organizations_.ApiOrganizationsEmployeesList(
            {.body = {.limit = 1, .organization_id = organization, .subject = logist}}, token);
        if (employees.items.empty())
            throw ApiError<400>{
                "invalid_logist", "В карточке клиента назначен недействующий логист"};
    }

    std::set<std::string> ids;

    std::set<std::string> parties;
    const auto addLocation = [&](const auto& value) {
        if (value)
            ids.insert(value->id);
    };
    const auto addParty = [&](const auto& value) {
        if (value)
            parties.insert(*value);
    };
    for (const auto& segment : segments) {
        if (segment.sea) {
            addLocation(segment.sea->pol);
            addLocation(segment.sea->pod);
            addParty(segment.sea->departure_terminal_id);
            addParty(segment.sea->arrival_terminal_id);
        }
        if (segment.air) {
            addLocation(segment.air->origin_airport);
            addLocation(segment.air->destination_airport);
            addParty(segment.air->carrier_id);
        }
    }

    std::map<std::string, models::TransportLocation> locations;
    if (!ids.empty()) {
        const auto response = transport_locations_.ApiV1RouteLocationsGetBatch(
            {.body = {.ids = std::vector<std::string>(ids.begin(), ids.end())}}, token);
        for (const auto& location : response.items)
            locations.emplace(location.id,
                models::ParseModelJson(json::ValueBuilder(location).ExtractValue(),
                    std::type_identity<models::TransportLocation>{}));
    }
    const auto resolve = [&](auto& value, models::TransportLocationKind kind) {
        if (!value)
            return;
        const auto found = locations.find(value->id);
        if (found == locations.end() || found->second.kind != kind)
            throw ApiError<400>{
                "invalid_route_location", "Выберите транспортный пункт из справочника"};
        value = found->second;
    };

    for (auto& segment : segments) {
        if (segment.sea) {
            resolve(segment.sea->pol, models::TransportLocationKind::SeaPort);
            resolve(segment.sea->pod, models::TransportLocationKind::SeaPort);
        }
        if (segment.air) {
            resolve(segment.air->origin_airport, models::TransportLocationKind::Airport);
            resolve(segment.air->destination_airport, models::TransportLocationKind::Airport);
        }
    }
    for (const auto& id : parties) {
        const auto party = GetCounterparty(token, id);
        if (party.archived)
            throw ApiError<400>{
                "invalid_route_counterparty", "Выберите действующего контрагента участка"};
    }
} catch (const platform::http::ErrorByStatus<403>&) {

    throw ApiError<403>{"reference_forbidden", "Нет доступа к связанным данным"};
} catch (const platform::http::ErrorByStatus<404>&) {
    throw ApiError<404>{"reference_not_found", "Связанный объект не найден в организации"};
} catch (const platform::http::CancelledError&) {
    throw;
} catch (const platform::http::ApiError&) {
    throw ApiError<502>{"reference_unavailable", "Не удалось получить связанные данные"};
} catch (const platform::http::TransportError&) {
    throw ApiError<502>{"reference_unavailable", "Сервис связанных данных недоступен"};
} catch (const platform::http::ContractError&) {
    throw ApiError<502>{"reference_unavailable", "Некорректный ответ сервиса связанных данных"};
}

models::RfqInput SalesDirectory::ResolveSupplierQuotes(const std::string& organization,
    std::string_view token, const models::Inquiry& inquiry, models::RfqInput input) const try {
    if (userver::utils::text::Trim(input.title).empty())
        throw ApiError<400>{"empty_title", "Укажите название запроса ставок"};
    ValidateSalesParties(organization, token, inquiry.customer_id, input.responsible_subject);
    if (input.status == models::RfqStatus::Ready && input.quotes.empty())
        throw ApiError<400>{"quotes_required", "Добавьте хотя бы одну котировку"};

    std::map<std::string, std::string> suppliers;

    std::map<std::string, std::string> labels;

    std::set<std::string> quote_ids;

    for (auto& quote : input.quotes) {
        if (!quote_ids.insert(quote.id).second)
            throw ApiError<400>{"duplicate_quote", "У котировок должны быть разные идентификаторы"};
        if (userver::utils::text::Trim(quote.title).empty())
            throw ApiError<400>{"empty_quote_title", "Укажите название варианта поставщика"};
        if (!suppliers.contains(quote.supplier_id)) {
            const auto supplier = GetCounterparty(token, quote.supplier_id);
            if (supplier.archived)
                throw ApiError<400>{"archived_supplier", "Поставщик в архиве"};
            suppliers.emplace(quote.supplier_id, supplier.name);
        }
        quote.supplier_name = suppliers.at(quote.supplier_id);
        std::set<std::string> lines;
        for (auto& line : quote.lines) {
            if (!lines.insert(line.id).second)
                throw ApiError<400>{"duplicate_line", "У строк должны быть разные идентификаторы"};
            if (!labels.contains(line.label_id)) {
                const auto label =
                    finance_.ApiV1FinancialLabelsGet({.body = {.id = line.label_id}}, token).label;
                if (label.archived ||
                    label.direction == clients::finance::models::FinancialLabelDirection::Income)
                    throw ApiError<400>{"invalid_label", "Выберите метку для расхода"};
                labels.emplace(line.label_id, label.name);
            }
            line.label_name = labels.at(line.label_id);
            platform::CalculateChargeAmounts(line.quantity_milli, line.price.price_minor,
                line.price.price_mode == models::PriceMode::Gross,
                line.price.vat_mode == models::VatMode::Exempt, line.price.vat_rate_basis_points);
        }
    }
    return input;
} catch (const platform::http::ErrorByStatus<403>&) {

    throw ApiError<403>{"reference_forbidden", "Нет доступа к справочникам котировок"};
} catch (const platform::http::ErrorByStatus<404>&) {
    throw ApiError<404>{"reference_not_found", "Связанный объект не найден в организации"};
} catch (const platform::http::CancelledError&) {
    throw;
} catch (const platform::http::ApiError&) {
    throw ApiError<502>{"reference_unavailable", "Не удалось получить справочники котировок"};
} catch (const platform::http::TransportError&) {
    throw ApiError<502>{"reference_unavailable", "Сервис справочников недоступен"};
} catch (const platform::http::ContractError&) {
    throw ApiError<502>{"reference_unavailable", "Некорректный ответ сервиса справочников"};
}

}  // namespace sales
