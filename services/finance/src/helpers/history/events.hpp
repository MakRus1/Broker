#pragma once
#include <helpers/history/changes.hpp>

namespace finance::helpers::history {
using Source = models::FinancialHistorySource;
using Action = models::FinancialHistoryAction;
using Document = models::FinancialHistoryDocument;
using FieldKey = models::FinancialHistoryField;
using Reference = models::FinancialHistoryReference;

inline bool Archived(const Json& doc) {
    return At(doc, "archived").As<bool>(false);
}

inline void Fields(std::vector<models::FinancialHistoryChange>& changes, const auto& fields,
    const Json& before, const Json& after, const std::optional<Reference>& subject = std::nullopt) {
    for (const auto& field : fields)
        AddChange(changes, field, before, after, subject);
}

inline void InvoiceLines(
    std::vector<models::FinancialHistoryChange>& changes, const Json& before, const Json& after) {
    const auto old = Indexed(At(before, "charges"), "id");
    const auto current = Indexed(At(after, "charges"), "id");
    std::set<std::string> ids;
    for (const auto& [id, row] : old)
        ids.insert(id);
    for (const auto& [id, row] : current)
        ids.insert(id);
    for (const auto& id : ids) {
        const auto previous = Find(old, id);
        const auto value = Find(current, id);
        const Reference subject{.id = id,
            .name = Scalar(At(current.contains(id) ? value : previous, "label_name")),
            .source = Source::ServiceCharge};
        if (!old.contains(id) || !current.contains(id)) {
            const models::FinancialHistoryValue included{
                .format = Format::Boolean, .value = "true"};
            changes.push_back(
                {.after = current.contains(id) ? std::optional{included} : std::nullopt,
                    .before = old.contains(id) ? std::optional{included} : std::nullopt,
                    .field = FieldKey::Charges,
                    .subject = subject});
        } else {
            Fields(changes, kChargeFields, previous, value, subject);
        }
    }
}

inline std::int64_t Allocated(const Json& snapshot, const std::string& invoice) {
    if (Archived(At(snapshot, "payment")))
        return 0;
    const auto allocations = Indexed(At(snapshot, "allocations"), "invoice_id");
    return At(Find(allocations, invoice), "amount_minor").As<std::int64_t>(0);
}

inline std::optional<models::FinancialHistoryValue> Money(std::int64_t value, const Json& doc) {
    if (value == 0)
        return std::nullopt;
    return models::FinancialHistoryValue{.currency = Scalar(At(doc, "payment.currency")),
        .format = Format::Money,
        .value = std::to_string(value)};
}

inline Document DocumentType(Source source, const Json& document) {
    if (source == Source::ServiceCharge)
        return Document::ServiceCharge;
    if (source == Source::Invoice)
        return Document::Invoice;
    return models::ParseEnumScalar(Name(document, "kind"), models::PaymentKind{}) ==
            models::PaymentKind::Receipt
        ? Document::Receipt
        : Document::Disbursement;
}

inline Action DocumentAction(std::int64_t revision, const Json& before, const Json& after) {
    if (revision == 1)
        return Action::Created;
    if (before.IsMissing() || before.IsNull())
        return Action::Updated;
    if (Archived(after) && !Archived(before))
        return Action::Archived;
    if (!Archived(after) && Archived(before))
        return Action::Restored;
    return Action::Updated;
}

inline models::FinancialHistoryEvent Event(models::FinancialHistoryCursor cursor,
    const std::string& actor, const Json& before, const Json& after, Source requested,
    const std::string& requested_id, const std::map<std::string, std::string>& invoices) {
    const auto document = cursor.source == Source::Invoice ? At(after, "invoice")
        : cursor.source == Source::Payment                 ? At(after, "payment")
                                                           : after;
    const auto previous = cursor.source == Source::Invoice ? At(before, "invoice")
        : cursor.source == Source::Payment                 ? At(before, "payment")
                                                           : before;
    models::FinancialHistoryEvent event{
        .action = DocumentAction(cursor.revision, previous, document),
        .actor = actor,
        .cursor = cursor,
        .document = DocumentType(cursor.source, document),
        .has_previous = !before.IsNull() && !before.IsMissing()};
    event.actor_name = Scalar(At(after, "actor_name"));
    if (cursor.source == Source::Invoice && requested == Source::ServiceCharge) {
        const auto old = Indexed(At(before, "charges"), "id");
        const auto current = Indexed(At(after, "charges"), "id");
        const bool was = old.contains(requested_id) && !Archived(At(before, "invoice"));
        const bool included = current.contains(requested_id) && !Archived(At(after, "invoice"));
        event.action = included ? Action::InvoiceLinked : Action::InvoiceUnlinked;
        event.reference = Reference{.id = cursor.entity_id,
            .name = Scalar(At(document, "number")),
            .source = Source::Invoice};
        event.changes.push_back(
            {.after = included ? std::optional{models::FinancialHistoryValue{
                                     .display_name = Scalar(At(document, "number")),
                                     .format = Format::Reference,
                                     .value = cursor.entity_id}}
                               : std::nullopt,
                .before = was ? std::optional{models::FinancialHistoryValue{
                                    .display_name = Scalar(At(previous, "number")),
                                    .format = Format::Reference,
                                    .value = cursor.entity_id}}
                              : std::nullopt,
                .field = FieldKey::Invoice});
        return event;
    }
    if (cursor.source == Source::Payment && requested == Source::Invoice) {
        const auto old = Allocated(before, requested_id);
        const auto current = Allocated(after, requested_id);
        event.action = current == 0 ? Action::PaymentUnallocated
            : old == 0              ? Action::PaymentAllocated
                                    : Action::PaymentReallocated;
        event.reference = Reference{.id = cursor.entity_id,
            .name = Scalar(At(document, "number")),
            .source = Source::Payment};
        event.changes.push_back({.after = Money(current, after),
            .before = Money(old, before),
            .field = FieldKey::Allocation});
        return event;
    }
    if (cursor.source == Source::ServiceCharge)
        Fields(event.changes, kChargeFields, previous, document);
    if (cursor.source == Source::Invoice) {
        Fields(event.changes, kInvoiceFields, previous, document);
        InvoiceLines(event.changes, before, after);
    }
    if (cursor.source == Source::Payment) {
        Fields(event.changes, kPaymentFields, previous, document);
        const auto old = Indexed(At(before, "allocations"), "invoice_id");
        const auto current = Indexed(At(after, "allocations"), "invoice_id");
        std::set<std::string> ids;
        for (const auto& [id, row] : old)
            ids.insert(id);
        for (const auto& [id, row] : current)
            ids.insert(id);
        for (const auto& id : ids) {
            const auto was = Allocated(before, id);
            const auto amount = Allocated(after, id);
            if (was == amount)
                continue;
            const auto name = invoices.find(id);
            event.changes.push_back({.after = Money(amount, after),
                .before = Money(was, before),
                .field = FieldKey::Allocation,
                .subject = Reference{.id = id,
                    .name = name == invoices.end() ? std::nullopt : std::optional{name->second},
                    .source = Source::Invoice}});
        }
    }
    return event;
}
}  // namespace finance::helpers::history
