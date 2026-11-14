#include <components/cities.hpp>
#include <views/api/geography/cities/view.hpp>

namespace web_bff::views::api::geography::cities {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    return Response200{.items = deps.cities.FindCities(request.body.query)};
}
}  // namespace web_bff::views::api::geography::cities
