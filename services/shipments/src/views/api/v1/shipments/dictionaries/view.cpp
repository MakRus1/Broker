#include <helpers/database.hpp>
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
        if (entry.Dictionary == "incoterms")
            result.incoterms.push_back(value);
        else if (entry.Dictionary == "carriage_terms")
            result.carriage_terms.push_back(value);
        else if (entry.Dictionary == "loading")
            result.loading.push_back(value);
        else if (entry.Dictionary == "route_type")
            result.route_types.push_back(value);
    }
    return result;
}
}  // namespace shipments::views::api::v1::shipments::dictionaries
