#include <components/cities.hpp>
#include <views/api/geography/cities/view.hpp>

namespace web_bff::views::api::geography::cities {
Response View::Handle(Request&& request, userver::server::request::RequestContext&, Deps deps) {
    try {
        return Response200{.items = deps.cities.Find(request.body.query)};
    } catch (const ::web_bff::CityLookupError& error) {
        return Response503{.error = error.Code, .message = error.what()};
    }
}
}  // namespace web_bff::views::api::geography::cities
