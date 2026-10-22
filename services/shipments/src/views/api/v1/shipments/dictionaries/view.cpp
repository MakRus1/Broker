#include <helpers/database.hpp>
#include <helpers/dictionaries.hpp>
#include <postgresql/types.hpp>
#include <views/api/v1/shipments/dictionaries/view.hpp>

namespace shipments::views::api::v1::shipments::dictionaries {
Response View::Handle(Request&&, userver::server::request::RequestContext&, Deps deps) {
    using namespace ::shipments::helpers;
    const auto rows =
        deps.pg_cluster->Execute(pg::ClusterHostType::kSlave, sql::kListShipmentDictionaries);
    Response200 result;
    for (const auto& entry : rows.AsSetOf<pgtypes::ShipmentDictionaryValueV1>()) {
        const models::DictionaryValue value{.code = entry.Code, .name = entry.Name};
        switch (ParseDictionary(entry.Dictionary)) {
        case ShipmentDictionary::kIncoterms:
            result.incoterms.push_back(value);
            break;
        case ShipmentDictionary::kCarriageTerms:
            result.carriage_terms.push_back(value);
            break;
        case ShipmentDictionary::kLoading:
            result.loading.push_back(value);
            break;
        case ShipmentDictionary::kRouteType:
            result.route_types.push_back(value);
            break;
        }
    }
    return result;
}
}  // namespace shipments::views::api::v1::shipments::dictionaries
