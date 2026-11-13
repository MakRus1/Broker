#pragma once
#include <clients/contracts/client.hpp>
#include <clients/counterparties/client.hpp>
#include <models/document_parties.hpp>
#include <models/document_parties_input.hpp>
#include <openapi/models.hpp>
#include <string_view>
#include <userver/components/component_base.hpp>

namespace finance {
class DocumentPartiesComponent final : public userver::components::ComponentBase {
  public:
    static constexpr std::string_view kName = "finance-document-parties";
    DocumentPartiesComponent(
        const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    static userver::yaml_config::Schema GetStaticConfigSchema();

    models::DocumentParties ResolveDocumentParties(
        std::string_view access_token, const models::DocumentPartiesInput& input) const;

    models::InvoicePrintData PrepareInvoicePrintData(
        std::string_view access_token, const models::InvoiceDetails& details) const;

  private:
    clients::counterparties::Client counterparties_;
    clients::contracts::Client contracts_;
};
}  // namespace finance

template <>
inline constexpr bool userver::components::kHasValidate<finance::DocumentPartiesComponent> = true;
